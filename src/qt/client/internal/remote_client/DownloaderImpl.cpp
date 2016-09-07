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

#include <unity/storage/qt/client/internal/remote_client/DownloaderImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/qt/client/Downloader.h>
#include <unity/storage/qt/client/File.h>
#include <unity/storage/qt/client/internal/remote_client/Handler.h>

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
namespace remote_client
{

DownloaderImpl::DownloaderImpl(QString const& download_id,
                               QDBusUnixFileDescriptor fd,
                               shared_ptr<File> const& file,
                               shared_ptr<ProviderInterface> const& provider)
    : DownloaderBase(file)
    , download_id_(download_id)
    , fd_(fd)
    , file_(file)
    , provider_(provider)
    , read_socket_(new QLocalSocket, [](QLocalSocket* s){ s->deleteLater(); })
{
    assert(!download_id.isEmpty());
    assert(fd.isValid());
    assert(provider);
    read_socket_->setSocketDescriptor(fd.fileDescriptor(), QLocalSocket::ConnectedState, QIODevice::ReadOnly);
}

DownloaderImpl::~DownloaderImpl()
{
    read_socket_->abort();
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
    auto reply = provider_->FinishDownload(download_id_);

    auto process_reply = [this](decltype(reply) const&, QFutureInterface<void>& qf)
    {
        qf.reportFinished();
    };

    auto handler = new Handler<void>(this, reply, process_reply);
    return handler->future();
}

QFuture<void> DownloaderImpl::cancel() noexcept
{
    read_socket_->abort();
    QString msg = "Downloader::finish_download(): download of " + file_->name() + " was cancelled";
    return make_exceptional_future(CancelledException(msg));
}

Downloader::SPtr DownloaderImpl::make_downloader(QString const& download_id,
                                                 QDBusUnixFileDescriptor fd,
                                                 shared_ptr<File> const& file,
                                                 shared_ptr<ProviderInterface> const& provider)
{
    auto impl = new DownloaderImpl(download_id, fd, file, provider);
    Downloader::SPtr downloader(new Downloader(impl));
    return downloader;
}


}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
