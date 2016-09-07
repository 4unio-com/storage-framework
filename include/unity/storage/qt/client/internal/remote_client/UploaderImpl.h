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

#include <QDBusUnixFileDescriptor>

class QLocalSocket;
class ProviderInterface;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Root;
class Uploader;

namespace internal
{
namespace remote_client
{

class UploaderImpl : public UploaderBase
{
public:
    UploaderImpl(QString const& upload_id,
                 QDBusUnixFileDescriptor fd,
                 int64_t size,
                 QString const& old_etag,
                 std::weak_ptr<Root> root,
                 std::shared_ptr<ProviderInterface> const& provider);
    ~UploaderImpl();

    virtual std::shared_ptr<QLocalSocket> socket() const override;
    virtual QFuture<std::shared_ptr<File>> finish_upload() override;
    virtual QFuture<void> cancel() noexcept override;

    static std::shared_ptr<Uploader> make_uploader(QString const& upload_id,
                                                   QDBusUnixFileDescriptor fd,
                                                   int64_t size,
                                                   QString const& old_etag,
                                                   std::weak_ptr<Root> root,
                                                   std::shared_ptr<ProviderInterface> const& provider);

private:
    enum State { uploading, finalized };

    QString upload_id_;
    QDBusUnixFileDescriptor fd_;
    QString old_etag_;
    std::shared_ptr<Root> root_;
    std::shared_ptr<ProviderInterface> provider_;
    std::shared_ptr<QLocalSocket> write_socket_;
    State state_;
};

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
