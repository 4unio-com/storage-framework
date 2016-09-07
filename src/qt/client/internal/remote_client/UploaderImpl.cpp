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

#include <unity/storage/qt/client/internal/remote_client/UploaderImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/qt/client/internal/remote_client/FileImpl.h>
#include <unity/storage/qt/client/internal/remote_client/Handler.h>
#include <unity/storage/qt/client/internal/remote_client/validate.h>
#include <unity/storage/qt/client/Uploader.h>

#include <cassert>

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

UploaderImpl::UploaderImpl(QString const& upload_id,
                           QDBusUnixFileDescriptor fd,
                           int64_t size,
                           QString const& old_etag,
                           weak_ptr<Root> root,
                           shared_ptr<ProviderInterface> const& provider)
    : UploaderBase(old_etag == "" ? ConflictPolicy::overwrite : ConflictPolicy::error_if_conflict, size)
    , upload_id_(upload_id)
    , fd_(fd)
    , old_etag_(old_etag)
    , root_(root.lock())
    , provider_(provider)
    , write_socket_(new QLocalSocket, [](QLocalSocket* s){ s->deleteLater(); })
    , state_(uploading)
{
    assert(!upload_id.isEmpty());
    assert(fd.isValid());
    assert(size >= 0);
    assert(root_);
    assert(provider);
    assert(fd.isValid());
    write_socket_->setSocketDescriptor(fd_.fileDescriptor(), QLocalSocket::ConnectedState, QIODevice::WriteOnly);
}

UploaderImpl::~UploaderImpl()
{
    if (state_ == uploading)
    {
        provider_->CancelUpload(upload_id_);
    }
}

shared_ptr<QLocalSocket> UploaderImpl::socket() const
{
    return write_socket_;
}

QFuture<shared_ptr<File>> UploaderImpl::finish_upload()
{
    state_ = finalized;

    auto reply = provider_->FinishUpload(upload_id_);
    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<shared_ptr<File>>& qf)
    {
        auto md = reply.value();
        try
        {
            validate("Uploader::finish_upload()", md);
        }
        catch (StorageException const& e)
        {
            qf.reportException(e);
            qf.reportFinished();
            return;
        }
        if (md.type != ItemType::file)
        {
            // TODO: log server error here
            QString msg = "Uploader::finish_upload(): impossible item type returned by server: "
                          + QString::number(int(md.type));
            qf.reportException(LocalCommsException(msg));
            qf.reportFinished();
            return;
        }
        qf.reportResult(FileImpl::make_file(md, root_));
        qf.reportFinished();
    };

    write_socket_->disconnectFromServer();
    auto handler = new Handler<shared_ptr<File>>(this, reply, process_reply);
    return handler->future();
}

QFuture<void> UploaderImpl::cancel() noexcept
{
    state_ = finalized;

    auto reply = provider_->CancelUpload(upload_id_);
    auto process_reply = [this](decltype(reply) const&, QFutureInterface<void>& qf)
    {
        qf.reportFinished();
    };

    write_socket_->abort();
    auto handler = new Handler<void>(this, reply, process_reply);
    return handler->future();
}

Uploader::SPtr UploaderImpl::make_uploader(QString const& upload_id,
                                           QDBusUnixFileDescriptor fd,
                                           int64_t size,
                                           QString const& old_etag,
                                           weak_ptr<Root> root,
                                           shared_ptr<ProviderInterface> const& provider)
{
    assert(provider);
    auto impl = new UploaderImpl(upload_id, fd, size, old_etag, root, provider);
    Uploader::SPtr uploader(new Uploader(impl));
    return uploader;
}

}  // namespace remote_client
}  // namespace intternal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
