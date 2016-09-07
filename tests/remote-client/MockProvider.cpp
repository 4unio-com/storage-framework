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

#include "MockProvider.h"

#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/metadata_keys.h>

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <chrono>
#include <thread>
#include <inttypes.h>

using namespace unity::storage;
using namespace unity::storage::provider;
using namespace std;

using boost::make_exceptional_future;
using boost::make_ready_future;

MockProvider::MockProvider()
{
}

MockProvider::MockProvider(string const& cmd)
    : cmd_(cmd)
{
}

boost::future<ItemList> MockProvider::roots(Context const&)
{
    ItemList roots =
    {
        {"root_id", {}, "Root", "etag", ItemType::root, {}}
    };
    return make_ready_future<ItemList>(roots);
}

boost::future<tuple<ItemList,string>> MockProvider::list(
    string const& item_id, string const& page_token,
    Context const&)
{
    if (item_id != "root_id")
    {
        string msg = string("Item::list(): no such item: \"") + item_id + "\"";
        return make_exceptional_future<tuple<ItemList,string>>(NotExistsException(msg, item_id));
    }
    if (page_token != "")
    {
        string msg = string("Item::list(): invalid page token: \"") + page_token + "\"";
        return make_exceptional_future<tuple<ItemList,string>>(LogicException("invalid page token"));
    }
    ItemList children =
    {
        {
            "child_id", { "root_id" }, "Child", "etag", ItemType::file,
            { { SIZE_IN_BYTES, 0 }, { LAST_MODIFIED_TIME, "2007-04-05T14:30Z" } }
        }
    };
    boost::promise<tuple<ItemList,string>> p;
    p.set_value(make_tuple(children, string()));
    return p.get_future();
}

boost::future<ItemList> MockProvider::lookup(
    string const& parent_id, string const& name, Context const&)
{
    if (parent_id != "root_id")
    {
        string msg = string("Folder::lookup(): no such item: \"") + parent_id + "\"";
        return make_exceptional_future<ItemList>(NotExistsException(msg, parent_id));
    }
    if (name != "Child")
    {
        string msg = string("Folder::lookup(): no such item: \"") + name + "\"";
        return make_exceptional_future<ItemList>(NotExistsException(msg, name));
    }
    ItemList children =
    {
        { "child_id", { "root_id" }, "Child", "etag", ItemType::file,
          { { SIZE_IN_BYTES, 0 }, { LAST_MODIFIED_TIME, "2007-04-05T14:30Z" } } }
    };
    return make_ready_future<ItemList>(children);
}

boost::future<Item> MockProvider::metadata(string const& item_id, Context const&)
{
    if (item_id == "root_id")
    {
        Item metadata{"root_id", {}, "Root", "etag", ItemType::root, {}};
        return make_ready_future<Item>(metadata);
    }
    else if (item_id == "child_id")
    {
        Item metadata
        {
            "child_id", { "root_id" }, "Child", "etag", ItemType::file,
            { { SIZE_IN_BYTES, 0 }, { LAST_MODIFIED_TIME, "2007-04-05T14:30Z" } }
        };
        return make_ready_future<Item>(metadata);
    }
    else if (item_id == "child_folder_id")
    {
        Item metadata{"child_folder_id", { "root_id" }, "Child_Folder", "etag", ItemType::folder, {}};
        return make_ready_future<Item>(metadata);
    }
    return make_exceptional_future<Item>(NotExistsException("metadata(): no such item: " + item_id, item_id));
}

boost::future<Item> MockProvider::create_folder(
    string const& parent_id, string const& name,
    Context const&)
{
    Item metadata{"new_folder_id", { parent_id }, name, "etag", ItemType::folder, {}};
    return make_ready_future<Item>(metadata);
}

string make_job_id()
{
    static int last_job_id = 0;
    return to_string(++last_job_id);
}

boost::future<unique_ptr<UploadJob>> MockProvider::create_file(
    string const&, string const&,
    int64_t, string const&, bool, Context const&)
{
    return make_ready_future<unique_ptr<UploadJob>>(new MockUploadJob(make_job_id()));
}

boost::future<unique_ptr<UploadJob>> MockProvider::update(
    string const&, int64_t, string const&, Context const&)
{
    return make_ready_future<unique_ptr<UploadJob>>(new MockUploadJob(make_job_id()));
}

boost::future<unique_ptr<DownloadJob>> MockProvider::download(
    string const&, Context const&)
{
    unique_ptr<DownloadJob> job(new MockDownloadJob(make_job_id()));
    const char contents[] = "Hello world";
    if (write(job->write_socket(), contents, sizeof(contents)) != sizeof(contents))
    {
        ResourceException e("download(): write failed", errno);
        job->report_error(make_exception_ptr(e));
        return make_exceptional_future<unique_ptr<DownloadJob>>(e);
    }
    job->report_complete();
    return make_ready_future(std::move(job));
}

boost::future<void> MockProvider::delete_item(
    string const&, Context const&)
{
    return make_ready_future();
}

boost::future<Item> MockProvider::move(
    string const& item_id, string const& new_parent_id,
    string const& new_name, Context const&)
{
    Item metadata{item_id, { new_parent_id }, new_name, "etag", ItemType::file, {}};
    return make_ready_future(metadata);
}

boost::future<Item> MockProvider::copy(
    string const&, string const& new_parent_id,
    string const& new_name, Context const&)
{
    Item metadata{"new_item_id", { new_parent_id }, new_name, "etag", ItemType::file, {}};
    return make_ready_future(metadata);
}

MockUploadJob::MockUploadJob()
    : UploadJob("some_id")
{
}

MockUploadJob::MockUploadJob(string const& cmd)
    : UploadJob("some_id")
    , cmd_(cmd)
{
}

boost::future<void> MockUploadJob::cancel()
{
    return make_ready_future();
}

boost::future<Item> MockUploadJob::finish()
{
    Item metadata
    {
        "some_id", { "root_id" }, "some_upload", "etag", ItemType::file,
        { { SIZE_IN_BYTES, 10 }, { LAST_MODIFIED_TIME, "2011-04-05T14:30:10.005Z" } }
    };
    return make_ready_future(metadata);
}

MockDownloadJob::MockDownloadJob()
    : DownloadJob("some_id")
{
}

MockDownloadJob::MockDownloadJob(string const& cmd)
    : DownloadJob("some_id")
    , cmd_(cmd)
{
}

boost::future<void> MockDownloadJob::cancel()
{
    return make_ready_future();
}

boost::future<void> MockDownloadJob::finish()
{
    return make_ready_future();
}
