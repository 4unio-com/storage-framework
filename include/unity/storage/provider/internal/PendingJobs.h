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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#pragma GCC diagnostic pop

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace unity
{
namespace storage
{
namespace provider
{

class DownloadJob;
class UploadJob;

namespace internal
{


class PendingJobs : public QObject
{
    Q_OBJECT

public:
    explicit PendingJobs(QDBusConnection const& bus, QObject *parent=nullptr);
    virtual ~PendingJobs();

    void add_download(QString const& client_bus_name, std::unique_ptr<DownloadJob> &&job);
    std::shared_ptr<DownloadJob> remove_download(QString const& client_bus_name, std::string const& download_id);

    void add_upload(QString const& client_bus_name, std::unique_ptr<UploadJob> &&job);
    std::shared_ptr<UploadJob> remove_upload(QString const& client_bus_name, std::string const& upload_id);

private Q_SLOTS:
    void service_disconnected(QString const& service_name);

private:
    void watch_peer(QString const& bus_name);
    void unwatch_peer(QString const& bus_name);

    template <typename Job>
    void cancel_job(std::shared_ptr<Job> const& job,
                    std::string const& identifier);

    std::mutex lock_;
    // Key is client_bus_name and upload or download ID.
    std::map<std::pair<QString,std::string>,std::shared_ptr<UploadJob>> uploads_;
    std::map<std::pair<QString,std::string>,std::shared_ptr<DownloadJob>> downloads_;

    QDBusServiceWatcher watcher_;
    std::map<QString,int> services_;

    Q_DISABLE_COPY(PendingJobs)
};

}
}
}
}
