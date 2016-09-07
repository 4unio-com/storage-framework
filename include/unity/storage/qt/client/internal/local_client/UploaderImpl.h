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

#pragma once

#include <unity/storage/qt/client/internal/UploaderBase.h>

#include <QFile>
#include <QFutureInterface>
#include <QThread>
#include <unity/util/ResourcePtr.h>

class QLocalSocket;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class File;
class Root;

namespace internal
{
namespace local_client
{

class UploadWorker : public QObject
{
    Q_OBJECT

public:
    UploadWorker(int read_fd,
                 std::weak_ptr<File> file,
                 int64_t size,
                 QString const& path,
                 ConflictPolicy policy,
                 std::weak_ptr<Root> root,
                 QFutureInterface<std::shared_ptr<File>>& qf,
                 QFutureInterface<void>& worker_initialized);
    virtual ~UploadWorker();

    void start_uploading() noexcept;

public Q_SLOTS:
    void do_finish();
    void do_cancel();

private Q_SLOTS:
    void on_bytes_ready();
    void on_read_channel_finished();

private:
    void read_and_write_chunk();
    void finalize();
    void handle_error(QString const& msg, int error_code);

    enum State { in_progress, finalized, cancelled, error };

    State state_ = in_progress;
    int read_fd_;
    std::shared_ptr<QLocalSocket> read_socket_;
    std::weak_ptr<File> file_;
    int64_t size_;
    int64_t bytes_read_;
    QString path_;
    std::weak_ptr<Root> root_;
    std::unique_ptr<QFile> output_file_;
    unity::util::ResourcePtr<int, std::function<void(int)>> tmp_fd_;
    ConflictPolicy policy_;
    QFutureInterface<std::shared_ptr<File>>& qf_;
    QFutureInterface<void>& worker_initialized_;
    QString error_msg_;
    int error_code_ = 0;
    bool use_linkat_ = true;
};

class UploadThread : public QThread
{
    Q_OBJECT

public:
    UploadThread(UploadWorker* worker);
    virtual void run() override;

private:
    UploadWorker* worker_;
};

class UploaderImpl : public UploaderBase
{
    Q_OBJECT

public:
    UploaderImpl(std::weak_ptr<File> file,
                 int64_t size,
                 QString const& path,
                 ConflictPolicy policy,
                 std::weak_ptr<Root> root);
    virtual ~UploaderImpl();

    virtual std::shared_ptr<QLocalSocket> socket() const override;
    virtual QFuture<std::shared_ptr<File>> finish_upload() override;
    virtual QFuture<void> cancel() noexcept override;

Q_SIGNALS:
    void do_finish();
    void do_cancel();

private:
    std::shared_ptr<QLocalSocket> write_socket_;
    QFutureInterface<std::shared_ptr<File>> qf_;
    std::unique_ptr<UploadThread> upload_thread_;
    std::unique_ptr<UploadWorker> worker_;
};

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
