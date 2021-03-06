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

#include <unity/storage/common.h>
#include <unity/storage/visibility.h>
#include <unity/storage/provider/Credentials.h>
#include <unity/storage/provider/Item.h>

#include <boost/thread/future.hpp>
#include <boost/variant.hpp>

#include <sys/types.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace unity
{
namespace storage
{
namespace provider
{

class DownloadJob;
class UploadJob;

struct UNITY_STORAGE_EXPORT Context
{
    uid_t uid;
    pid_t pid;
    std::string security_label;

    Credentials credentials;
};

class UNITY_STORAGE_EXPORT ProviderBase : public std::enable_shared_from_this<ProviderBase>
{
public:
    ProviderBase();
    virtual ~ProviderBase();

    ProviderBase(ProviderBase const& other) = delete;
    ProviderBase& operator=(ProviderBase const& other) = delete;

    virtual boost::future<ItemList> roots(std::vector<std::string> const& keys, Context const& context) = 0;
    virtual boost::future<std::tuple<ItemList,std::string>> list(
        std::string const& item_id, std::string const& page_token,
        std::vector<std::string> const& keys,
        Context const& context) = 0;
    virtual boost::future<ItemList> lookup(
        std::string const& parent_id, std::string const& name, std::vector<std::string> const& keys,
        Context const& context) = 0;
    virtual boost::future<Item> metadata(std::string const& item_id, std::vector<std::string> const& keys,
        Context const& context) = 0;

    virtual boost::future<Item> create_folder(
        std::string const& parent_id, std::string const& name, std::vector<std::string> const& keys,
        Context const& context) = 0;

    virtual boost::future<std::unique_ptr<UploadJob>> create_file(
        std::string const& parent_id, std::string const& name,
        int64_t size, std::string const& content_type, bool allow_overwrite, std::vector<std::string> const& keys,
        Context const& context) = 0;
    virtual boost::future<std::unique_ptr<UploadJob>> update(
        std::string const& item_id, int64_t size, std::string const& old_etag, std::vector<std::string> const& keys,
        Context const& context) = 0;

    virtual boost::future<std::unique_ptr<DownloadJob>> download(
        std::string const& item_id, std::string const& match_etag,
        Context const& context) = 0;

    virtual boost::future<void> delete_item(
        std::string const& item_id, Context const& context) = 0;
    virtual boost::future<Item> move(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name, std::vector<std::string> const& keys, Context const& context) = 0;
    virtual boost::future<Item> copy(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name, std::vector<std::string> const& keys, Context const& context) = 0;
};

}
}
}
