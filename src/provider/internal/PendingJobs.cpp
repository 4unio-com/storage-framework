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

#include <unity/storage/provider/internal/PendingJobs.h>
#include <unity/storage/provider/DownloadJob.h>
#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/UploadJob.h>
#include <unity/storage/provider/internal/DownloadJobImpl.h>
#include <unity/storage/provider/internal/MainLoopExecutor.h>
#include <unity/storage/provider/internal/UploadJobImpl.h>

#include <cassert>
#include <cstdio>
#include <stdexcept>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

PendingJobs::PendingJobs(QDBusConnection const& bus, QObject *parent)
    : QObject(parent)
{
    watcher_.setConnection(bus);
    watcher_.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(&watcher_, &QDBusServiceWatcher::serviceUnregistered,
            this, &PendingJobs::service_disconnected);
}

PendingJobs::~PendingJobs()
{
    for (const auto& pair : downloads_)
    {
        cancel_job(pair.second, "download " + pair.second->download_id());
    }
    for (const auto& pair : uploads_)
    {
        cancel_job(pair.second, "upload " + pair.second->upload_id());
    }
}

void PendingJobs::add_download(QString const& client_bus_name,
                               unique_ptr<DownloadJob> &&job)
{
    lock_guard<mutex> guard(lock_);

    assert(!client_bus_name.isEmpty() && client_bus_name[0] == ':');
    const auto job_id = make_pair(client_bus_name, job->download_id());
    assert(downloads_.find(job_id) == downloads_.end());

    shared_ptr<DownloadJob> j(std::move(job));
    downloads_.emplace(job_id, j);
    watch_peer(client_bus_name);
}

shared_ptr<DownloadJob> PendingJobs::remove_download(QString const& client_bus_name,
                                                     string const& download_id)
{
    lock_guard<mutex> guard(lock_);

    auto it = downloads_.find({client_bus_name, download_id});
    if (it == downloads_.cend())
    {
        throw LogicException("No such download: " + download_id);
    }
    auto job = it->second;
    downloads_.erase(it);
    unwatch_peer(client_bus_name);
    return job;
}

void PendingJobs::add_upload(QString const& client_bus_name,
                             unique_ptr<UploadJob> &&job)
{
    lock_guard<mutex> guard(lock_);

    assert(!client_bus_name.isEmpty() && client_bus_name[0] == ':');
    const auto job_id = make_pair(client_bus_name, job->upload_id());
    assert(uploads_.find(job_id) == uploads_.end());

    shared_ptr<UploadJob> j(std::move(job));
    uploads_.emplace(job_id, j);
    watch_peer(client_bus_name);
}

shared_ptr<UploadJob> PendingJobs::remove_upload(QString const& client_bus_name,
                                                 string const& upload_id)
{
    lock_guard<mutex> guard(lock_);

    auto it = uploads_.find({client_bus_name, upload_id});
    if (it == uploads_.cend())
    {
        throw LogicException("No such upload: " + upload_id);
    }
    auto job = it->second;
    uploads_.erase(it);
    unwatch_peer(client_bus_name);
    return job;
}

void PendingJobs::watch_peer(QString const& bus_name)
{
    auto it = services_.find(bus_name);
    if (it != services_.end())
    {
        it->second++;
    }
    else
    {
        watcher_.addWatchedService(bus_name);
        services_[bus_name] = 1;
    }
}

void PendingJobs::unwatch_peer(QString const& bus_name)
{
    auto it = services_.find(bus_name);
    if (it == services_.end())
    {
        return;
    }
    it->second--;
    if (it->second == 0)
    {
        services_.erase(it);
        watcher_.removeWatchedService(bus_name);
    }
}

void PendingJobs::service_disconnected(QString const& service_name)
{
    lock_guard<mutex> guard(lock_);

    services_.erase(service_name);
    watcher_.removeWatchedService(service_name);

    const auto lower = make_pair(service_name, string());

    for (auto it = downloads_.lower_bound(lower);
         it != downloads_.cend() && it->first.first == service_name; )
    {
        auto job = it->second;
        it = downloads_.erase(it);
        cancel_job(job, "download " + job->download_id());
    }

    for (auto it = uploads_.lower_bound(lower);
         it != uploads_.cend() && it->first.first == service_name; )
    {
        auto job = it->second;
        it = uploads_.erase(it);
        cancel_job(job, "upload " + job->upload_id());
    }
}

template<typename Job>
void PendingJobs::cancel_job(shared_ptr<Job> const& job, string const& identifier)
{
    auto f = job->p_->cancel(*job);
    // This continuation also ensures that the job remains
    // alive until the cancel method has completed.
    auto cancel_future = std::make_shared<boost::future<void>>();
    *cancel_future = f.then(
        EXEC_IN_MAIN
        [job, identifier, cancel_future](decltype(f) f) {
            try
            {
                f.get();
            }
            catch (std::exception const& e)
            {
                fprintf(stderr, "Error cancelling job '%s': %s\n",
                        identifier.c_str(), e.what());
            }

            // Break the reference cycle between the continuation
            // future and closure, while making sure the future
            // survives long enough to be marked ready.
            auto fut = std::make_shared<boost::future<void>>(std::move(*cancel_future));
            MainLoopExecutor::instance().submit([fut]{});
        });
}

}
}
}
}
