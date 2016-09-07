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

#include <unity/storage/qt/client/internal/local_client/DownloaderImpl.h>

#include <unity/storage/internal/safe_strerror.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/File.h>
#include <unity/storage/qt/client/internal/make_future.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#include <QLocalSocket>
#pragma GCC diagnostic pop

#include <cassert>

#include <sys/socket.h>

using namespace unity::storage::qt::client;
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

DownloadWorker::DownloadWorker(int write_fd,
                               QString const& filename,
                               QFutureInterface<void>& qf,
                               QFutureInterface<void>& worker_initialized)
    : write_fd_(write_fd)
    , filename_(filename)
    , qf_(qf)
    , worker_initialized_(worker_initialized)
{
    assert(write_fd >= 0);
    qf_.reportStarted();
    worker_initialized_.reportStarted();
}

void DownloadWorker::start_downloading() noexcept
{
    write_socket_.reset(new QLocalSocket);
    write_socket_->setSocketDescriptor(write_fd_, QLocalSocket::ConnectedState, QIODevice::WriteOnly);

    // We should be able to close the read channel of the write socket,
    // but doing this causes the disconnected signal to go AWOL.
    // Possibly a problem wit QLocalSocket.
    // shutdown(write_fd_, SHUT_RD);

    // Monitor write socket for ready-to-write, disconnected, and error events.
    connect(write_socket_.get(), &QLocalSocket::bytesWritten, this, &DownloadWorker::on_bytes_written);
    connect(write_socket_.get(), &QLocalSocket::disconnected, this, &DownloadWorker::on_disconnected);
    connect(write_socket_.get(), static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
            this, &DownloadWorker::on_error);

    // Open file for reading.
    input_file_.reset(new QFile(filename_));
    if (!input_file_->open(QIODevice::ReadOnly))
    {
        // LCOV_EXCL_START
        handle_error("cannot open " + filename_ + ": " + input_file_->errorString(), input_file_->error());
        return;
        // LCOV_EXCL_STOP
    }
    bytes_to_write_ = input_file_->size();

    worker_initialized_.reportFinished();

    if (bytes_to_write_ == 0)
    {
        write_socket_->disconnectFromServer();  // So the client gets EOF for empty files.
    }
    else
    {
        read_and_write_chunk();  // Kick off the read-write cycle.
    }
}

// Called once we know the outcome of the download, or via a signal when the client
// calls finish_download(). This makes the future ready with the appropriate
// result or error information.

void DownloadWorker::do_finish()
{
    switch (state_)
    {
        case in_progress:
        {
            if (bytes_to_write_ > 0)
            {
                // Still unwrittten data left, caller abandoned download early without cancelling.
                auto file_size = input_file_->size();
                auto written = file_size - bytes_to_write_;
                QString msg = "Downloader::finish_download(): method called too early, file "
                              + filename_ + " has size " + QString::number(file_size) + ", but only "
                              + QString::number(written) + " byte";
                msg += written == 1 ? " was" : "s were";
                msg += " consumed.";
                qf_.reportException(LogicException(msg));
            }
            else
            {
                state_ = finalized;
            }
            break;
        }
        case finalized:
        {
            abort();  // LCOV_EXCL_LINE  // Impossible. If we get here, our logic is broken.
        }
        case cancelled:
        {
            QString msg = "Downloader::finish_download(): download of " + filename_ + " was cancelled";
            qf_.reportException(CancelledException(msg));
            break;
        }
        case error:
        {
            qf_.reportException(ResourceException(error_msg_, error_code_));
            break;
        }
        default:
        {
            abort();  // LCOV_EXCL_LINE  // Impossible
        }
    }
    qf_.reportFinished();
    QThread::currentThread()->quit();
}

// Called via signal from the client to stop things.

void DownloadWorker::do_cancel()
{
    if (state_ == in_progress)
    {
        disconnect(write_socket_.get(), nullptr, this, nullptr);
        write_socket_->abort();
        bytes_to_write_ = 0;
        state_ = cancelled;
        do_finish();
    }
}

// Called each time we get rid of a chunk of data, to kick off the next chunk.

void DownloadWorker::on_bytes_written(qint64 bytes)
{
    bytes_to_write_ -= bytes;
    assert(bytes_to_write_ >= 0);
    if (bytes_to_write_ == 0)
    {
        input_file_->close();
        write_socket_->disconnectFromServer();
    }
    else
    {
        read_and_write_chunk();
    }
}

// Sets the outcome of the download in the future once we have written
// the last of the data and have disconnected.

void DownloadWorker::on_disconnected()
{
    do_finish();
}

void DownloadWorker::on_error()
{
    disconnect(write_socket_.get(), nullptr, this, nullptr);
    handle_error(write_socket_->errorString(), write_socket_->error());
}

// Read the next chunk of data from the input file and write it to the socket.

void DownloadWorker::read_and_write_chunk()
{
    static qint64 constexpr READ_SIZE = 64 * 1024;

    QByteArray buf;
    buf.resize(READ_SIZE);
    auto bytes_read = input_file_->read(buf.data(), buf.size());
    if (bytes_read == -1)
    {
        // LCOV_EXCL_START
        handle_error(filename_ + ": read error: " + input_file_->errorString(), input_file_->error());
        return;
        // LCOV_EXCL_STOP
    }
    buf.resize(bytes_read);

    auto bytes_written = write_socket_->write(buf);
    if (bytes_written == -1)
    {
        // LCOV_EXCL_START
        handle_error(filename_ + ": socket error: " + write_socket_->errorString(), write_socket_->error());
        // LCOV_EXCL_STOP
    }
    else if (bytes_written != bytes_read)
    {
        // LCOV_EXCL_START
        QString msg = filename_ + ": write error, requested " + bytes_read + " B, but wrote only "
                      + bytes_written + " B.";
        handle_error(msg, 0);
        // LCOV_EXCL_STOP
    }
}

void DownloadWorker::handle_error(QString const& msg, int error_code)
{
    if (state_ == in_progress)
    {
        write_socket_->abort();
    }
    state_ = error;
    error_msg_ = "Downloader: " + msg;
    error_code_ = error_code;
    do_finish();
}

DownloadThread::DownloadThread(DownloadWorker* worker)
    : worker_(worker)
{
}

void DownloadThread::run()
{
    worker_->start_downloading();
    exec();
}

DownloaderImpl::DownloaderImpl(weak_ptr<File> file)
    : DownloaderBase(file)
    , read_socket_(new QLocalSocket, [](QLocalSocket* s){ s->deleteLater(); })
{
    // Set up socket pair.
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if (rc == -1)
    {
        // LCOV_EXCL_START
        QString msg = "Downloader: cannot create socket pair: "
                      + QString::fromStdString(storage::internal::safe_strerror(errno));
        qf_.reportException(ResourceException(msg, errno));
        qf_.reportFinished();
        return;
        // LCOV_EXCL_STOP
    }

    // Read socket is for the client.
    read_socket_->setSocketDescriptor(fds[0], QLocalSocket::ConnectedState, QIODevice::ReadOnly);

    // We should be able to close the write channel of the client-side read socket, but
    // doing this causes the client to never see the readyRead signal.
    // Possibly a problem with QLocalSocket.
    // shutdown(fds[0], SHUT_WR);

    // Create worker and connect slots, so we can signal the worker when the client calls
    // finish_download() or cancel().
    QFutureInterface<void> worker_initialized;
    worker_.reset(new DownloadWorker(fds[1], file_->native_identity(), qf_, worker_initialized));
    connect(this, &DownloaderImpl::do_finish, worker_.get(), &DownloadWorker::do_finish);
    connect(this, &DownloaderImpl::do_cancel, worker_.get(), &DownloadWorker::do_cancel);

    // Create download thread and make sure that worker slots are called from the download thread.
    download_thread_.reset(new DownloadThread(worker_.get()));
    worker_->moveToThread(download_thread_.get());

    download_thread_->start();
    worker_initialized.future().waitForFinished();
}

DownloaderImpl::~DownloaderImpl()
{
    if (download_thread_->isRunning())
    {
        Q_EMIT do_cancel();
        download_thread_->wait();
    }
}

shared_ptr<File> DownloaderImpl::file() const
{
    return file_;
}

shared_ptr<QLocalSocket> DownloaderImpl::socket() const
{
    return read_socket_;
}

QFuture<void> DownloaderImpl::finish_download()
{
    Q_EMIT do_finish();
    return qf_.future();
}

QFuture<void> DownloaderImpl::cancel() noexcept
{
    Q_EMIT do_cancel();
    return qf_.future();
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
