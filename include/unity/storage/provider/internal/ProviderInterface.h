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
 * Authors: James Henstridge <james.henstridge@canonical.com>
 */

#pragma once

#include <unity/storage/internal/ItemMetadata.h>
#include <unity/storage/provider/internal/Handler.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <QObject>
#include <QList>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#pragma GCC diagnostic pop

#include <map>
#include <memory>

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

class AccountData;

class ProviderInterface : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    ProviderInterface(std::shared_ptr<AccountData> const& account_data,
                      QObject *parent=nullptr);
    ~ProviderInterface();

private:
    typedef unity::storage::internal::ItemMetadata IMD;  // To keep things readable

public Q_SLOTS:
    QList<IMD> Roots();
    QList<IMD> List(QString const& item_id, QString const& page_token, QString& next_token);
    QList<IMD> Lookup(QString const& parent_id, QString const& name);
    IMD Metadata(QString const& item_id);
    IMD CreateFolder(QString const& parent_id, QString const& name);
    QString CreateFile(QString const& parent_id, QString const& name, int64_t size, QString const& content_type, bool allow_overwrite, QDBusUnixFileDescriptor& file_descriptor);
    QString Update(QString const& item_id, int64_t size, QString const& old_etag, QDBusUnixFileDescriptor& file_descriptor);
    IMD FinishUpload(QString const& upload_id);
    void CancelUpload(QString const& upload_id);
    QString Download(QString const& item_id, QDBusUnixFileDescriptor& file_descriptor);
    void FinishDownload(QString const& download_id);
    void Delete(QString const& item_id);
    IMD Move(QString const& item_id, QString const& new_parent_id, QString const& new_name);
    IMD Copy(QString const& item_id, QString const& new_parent_id, QString const& new_name);

private Q_SLOTS:
    void request_finished();

private:
    void queue_request(Handler::Callback callback);

    std::shared_ptr<AccountData> const account_;
    std::map<Handler*, std::unique_ptr<Handler>> requests_;

    Q_DISABLE_COPY(ProviderInterface)
};

}
}
}
}
