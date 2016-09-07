/*
 * Copyright (C) 2016 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Michi Henning <michi.henning@canonical.com>
 */

#include <unity/storage/qt/client/internal/local_client/UploaderImpl.h>

#include <unity/storage/internal/safe_strerror.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/File.h>
#include <unity/storage/qt/client/internal/local_client/FileImpl.h>
#include <unity/storage/qt/client/internal/local_client/tmpfile_prefix.h>
#include <unity/storage/qt/client/internal/make_future.h>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#include <QLocalSocket>
#pragma GCC diagnostic pop

#include <cassert>

#include <fcntl.h>
#include <sys/socket.h>

using namespace std;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{
namespace internal
{
namespace local_client
{

UploadWorker::UploadWorker(int read_fd,
                           weak_ptr<File> file,
                           int64_t size,
                           QString const& path,
                           ConflictPolicy policy,
                           weak_ptr<Root> root,
                           QFutureInterface<shared_ptr<File>>& qf,
                           QFutureInterface<void>& worker_initialized)
    : read_fd_(read_fd)
    , file_(file)
    , size_(size)
    , bytes_read_(0)
    , path_(path)
    , root_(root)
    , tmp_fd_([](int fd){ if (fd != -1) ::close(fd); })
    , policy_(policy)
    , qf_(qf)
    , worker_initialized_(worker_initialized)
{
    assert(read_fd > 0);
    assert(size >= 0);
    qf_.reportStarted();
    worker_initialized_.reportStarted();
}

UploadWorker::~UploadWorker()
{
    if (state_ == error && !use_linkat_ && output_file_)
    {
        output_file_->remove();  // LCOV_EXCL_LINE
    }
}

void UploadWorker::start_uploading() noexcept
{
    read_socket_.reset(new QLocalSocket);
    read_socket_->setSocketDescriptor(read_fd_, QLocalSocket::ConnectedState, QIODevice::ReadOnly);

    // We should be able to close the write channel of the client-side read socket, but
    // doing this causes the client to never see the readyRead signal.
    // Possibly a problem with QLocalSocket.
    // shutdown(read_fd_, SHUT_WR);

    // Monitor read socket for ready-to-read, disconnected, and error events.
    connect(read_socket_.get(), &QLocalSocket::readyRead, this, &UploadWorker::on_bytes_ready);
    connect(read_socket_.get(), &QIODevice::readChannelFinished, this, &UploadWorker::on_read_channel_finished);

    using namespace boost::filesystem;

    // Open tmp file for writing.
    auto parent_path = path(path_.toStdString()).parent_path();
    tmp_fd_.reset(open(parent_path.native().c_str(), O_TMPFILE | O_WRONLY, 0600));
    if (tmp_fd_.get() == -1)
    {
        // LCOV_EXCL_START
        // Some kernels on the phones don't support O_TMPFILE and return various errno values when this fails.
        // So, if anything at all goes wrong, we fall back on conventional temp file creation and
        // produce a hard error if that doesn't work either.
        // Note that, in this case, the temp file retains its name in the file system. Not nice because,
        // if the client dies at the wrong moment, we leave the temp file behind.
        use_linkat_ = false;
        string tmpfile = parent_path.native() + "/" + TMPFILE_PREFIX + "XXXXXX";
        tmp_fd_.reset(mkstemp(const_cast<char*>(tmpfile.data())));
        if (tmp_fd_.get() == -1)
        {
            int error_code = errno;
            worker_initialized_.reportFinished();
            QString msg = "cannot create temp file \"" + QString::fromStdString(tmpfile)
                          + "\": " + QString::fromStdString(storage::internal::safe_strerror(errno));
            handle_error(msg, error_code);
            return;
        }
        output_file_.reset(new QFile(QString::fromStdString(tmpfile)));
        output_file_->open(QIODevice::WriteOnly);
        // LCOV_EXCL_STOP
    }
    else
    {
        output_file_.reset(new QFile);
        output_file_->open(tmp_fd_.get(), QIODevice::WriteOnly, QFileDevice::DontCloseHandle);
    }

    worker_initialized_.reportFinished();
}

// Called once we know the outcome of the upload, or via a signal when the client
// calls finish_upload(). This makes the future ready with the appropriate
// result or error information. If the client has not disconnected
// yet, we don't touch the future; it becomes ready once we receive the disconnected signal.

void UploadWorker::do_finish()
{
    switch (state_)
    {
        case in_progress:
        {
            if (bytes_read_ != size_)
            {
                QString msg = "Uploader::finish_upload(): " + path_ + ": upload size of " + QString::number(size_)
                              + " does not match actual number of bytes read: " + QString::number(bytes_read_);
                qf_.reportException(LogicException(msg));
                qf_.reportFinished();
            }
            else
            {
                state_ = finalized;
                finalize();
            }
            break;
        }
        case finalized:
        {
            abort();  // LCOV_EXCL_LINE  // Impossible. If we get here, our logic is broken.
        }
        case cancelled:
        {
            QString msg = "Uploader::finish_upload(): upload was cancelled";
            qf_.reportException(CancelledException(msg));
            qf_.reportFinished();
            break;
        }
        case error:
        {
            // LCOV_EXCL_START
            qf_.reportException(ResourceException(error_msg_, error_code_));
            qf_.reportFinished();
            break;
            // LCOV_EXCL_STOP
        }
        default:
        {
            abort();  // LCOV_EXCL_LINE  // Impossible
        }
    }
    if (qf_.future().isFinished())
    {
        QThread::currentThread()->quit();
    }
}

// Called via signal from the client to stop things.

void UploadWorker::do_cancel()
{
    if (state_ == in_progress)
    {
        disconnect(read_socket_.get(), nullptr, this, nullptr);
        read_socket_->abort();
        state_ = cancelled;
        do_finish();
    }
}

void UploadWorker::on_bytes_ready()
{
    auto buf = read_socket_->read(read_socket_->bytesAvailable());
    if (buf.size() != 0)
    {
        bytes_read_ += buf.size();
        auto bytes_written = output_file_->write(buf);
        if (bytes_written == -1)
        {
            handle_error("socket error: " + output_file_->errorString(), output_file_->error());  // LCOV_EXCL_LINE
        }
        else if (bytes_written != buf.size())
        {
            // LCOV_EXCL_START
            QString msg = "write error, requested " + QString::number(buf.size()) + " B, but wrote only "
                          + bytes_written + " B.";
            handle_error(msg, 0);
            // LCOV_EXCL_STOP
        }
    }
}

void UploadWorker::on_read_channel_finished()
{
    on_bytes_ready();  // In case there is still buffered data to be read.
    do_finish();
}

void UploadWorker::finalize()
{
    auto file = file_.lock();
    shared_ptr<FileImpl> impl;
    if (file)
    {
        // Upload is for a pre-existing file.
        impl = dynamic_pointer_cast<FileImpl>(file->p_);

        if (impl->has_conflict())
        {
            state_ = error;
            qf_.reportException(ConflictException("Uploader::finish_upload(): ETag mismatch"));
            qf_.reportFinished();
            return;
        }
    }
    else
    {
        // This uploader was returned by FolderImpl::create_file().
        int fd = open(path_.toStdString().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);  // Fails if path already exists.
        if (fd == -1)
        {
            state_ = error;
            QString msg = "Uploader::finish_upload(): item with name \"" + path_ + "\" exists already";
            QString name = QString::fromStdString(boost::filesystem::path(path_.toStdString()).filename().native());
            qf_.reportException(ExistsException(msg, path_, name));
            qf_.reportFinished();
            return;
        }
        if (close(fd) == -1)
        {
            // LCOV_EXCL_START
            state_ = error;
            QString msg = "Uploader::finish_upload(): cannot close tmp file: "
                          + QString::fromStdString(storage::internal::safe_strerror(errno));
            qf_.reportException(ResourceException(msg, errno));
            qf_.reportFinished();
            return;
            // LCOV_EXCL_STOP
        }

        file = FileImpl::make_file(path_, root_);
        impl = dynamic_pointer_cast<FileImpl>(file->p_);
    }

    if (!output_file_->flush())
    {
        // LCOV_EXCL_START
        state_ = error;
        QString msg = "Uploader::finish_upload(): cannot flush output file: " + output_file_->errorString();
        qf_.reportException(ResourceException(msg, output_file_->error()));
        qf_.reportFinished();
        return;
        // LCOV_EXCL_STOP
    }

    // Link the anonymous tmp file into the file system.
    auto new_path = file->native_identity().toStdString();
    if (use_linkat_)
    {
        auto old_path = string("/proc/self/fd/") + std::to_string(tmp_fd_.get());
        ::unlink(new_path.c_str());  // linkat() will not remove existing file: http://lwn.net/Articles/559969/
        if (linkat(-1, old_path.c_str(), tmp_fd_.get(), new_path.c_str(), AT_SYMLINK_FOLLOW) == -1)
        {
            // LCOV_EXCL_START
            int error_code = errno;
            state_ = error;
            QString msg = "Uploader::finish_upload(): linkat \"" + QString::fromStdString(old_path)
                          + "\" to \"" + file->native_identity() + "\" failed: "
                          + QString::fromStdString(storage::internal::safe_strerror(errno));
            qf_.reportException(ResourceException(msg, error_code));
            qf_.reportFinished();
            return;
            // LCOV_EXCL_STOP
        }
    }
    else
    {
        // LCOV_EXCL_START
        auto old_path = output_file_->fileName().toStdString();
        if (rename(old_path.c_str(), new_path.c_str()) == -1)
        {
            int error_code = errno;
            state_ = error;
            QString msg = "Uploader::finish_upload(): rename \"" + QString::fromStdString(old_path)
                          + "\" to \"" + file->native_identity() + "\" failed: "
                          + QString::fromStdString(storage::internal::safe_strerror(errno));
            qf_.reportException(ResourceException(msg, error_code));
            qf_.reportFinished();
            return;
        }
        // LCOV_EXCL_STOP
    }

    state_ = finalized;
    output_file_->close();
    impl->set_timestamps();
    qf_.reportResult(file);
    qf_.reportFinished();
}

// LCOV_EXCL_START
void UploadWorker::handle_error(QString const& msg, int error_code)
{
    if (state_ == in_progress)
    {
        output_file_->close();
        read_socket_->abort();
    }
    state_ = error;
    error_msg_ = "Uploader: " + msg;
    error_code_ = error_code;
    do_finish();
}
// LCOV_EXCL_STOP

UploadThread::UploadThread(UploadWorker* worker)
    : worker_(worker)
{
}

void UploadThread::run()
{
    worker_->start_uploading();
    exec();
}

UploaderImpl::UploaderImpl(weak_ptr<File> file,
                           int64_t size,
                           QString const& path,
                           ConflictPolicy policy,
                           weak_ptr<Root> root)
    : UploaderBase(policy, size)
    , write_socket_(new QLocalSocket, [](QLocalSocket* s){ s->deleteLater(); })
{
    // Set up socket pair.
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if (rc == -1)
    {
        // LCOV_EXCL_START
        QString msg = "Uploader: cannot create socket pair: "
                      + QString::fromStdString(storage::internal::safe_strerror(errno));
        qf_.reportException(ResourceException(msg, errno));
        qf_.reportFinished();
        return;
        // LCOV_EXCL_STOP
    }

    // Write socket is for the client.
    write_socket_->setSocketDescriptor(fds[1], QLocalSocket::ConnectedState, QIODevice::WriteOnly);

    // We should be able to close the read channel of the write socket,
    // but doing this causes the disconnected signal to go AWOL.
    // Possibly a problem wit QLocalSocket.
    // shutdown(fds[1], SHUT_RD);

    // Create worker and connect slots, so we can signal the worker when the client calls
    // finish_download() or cancel();
    QFutureInterface<void> worker_initialized;
    worker_.reset(new UploadWorker(fds[0], file, size, path, policy, root, qf_, worker_initialized));
    connect(this, &UploaderImpl::do_finish, worker_.get(), &UploadWorker::do_finish);
    connect(this, &UploaderImpl::do_cancel, worker_.get(), &UploadWorker::do_cancel);

    // Create upload thread and make sure that worker slots are called from the upload thread.
    upload_thread_.reset(new UploadThread(worker_.get()));
    worker_->moveToThread(upload_thread_.get());

    upload_thread_->start();

    worker_initialized.waitForFinished();
}

UploaderImpl::~UploaderImpl()
{
    if (upload_thread_->isRunning())
    {
        Q_EMIT do_cancel();
        upload_thread_->wait();
    }
}

shared_ptr<QLocalSocket> UploaderImpl::socket() const
{
    return write_socket_;
}

QFuture<File::SPtr> UploaderImpl::finish_upload()
{
    if (write_socket_->state() == QLocalSocket::ConnectedState)
    {
        write_socket_->disconnectFromServer();
    }
    return qf_.future();
}

QFuture<void> UploaderImpl::cancel() noexcept
{
    Q_EMIT do_cancel();
    write_socket_->abort();
    return qf_.future();
}

}  // namespace local_client
}  // namespace intternal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
