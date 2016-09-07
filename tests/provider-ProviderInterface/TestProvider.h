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

#include <unity/storage/provider/ProviderBase.h>

using namespace std;
using unity::storage::provider::Context;
using unity::storage::provider::DownloadJob;
using unity::storage::provider::ProviderBase;
using unity::storage::provider::Item;
using unity::storage::provider::ItemList;
using unity::storage::provider::UploadJob;

class TestProvider : public ProviderBase {
public:
    boost::future<ItemList> roots(Context const& ctx) override;
    boost::future<tuple<ItemList,string>> list(
        string const& item_id, string const& page_token,
        Context const& ctx) override;
    boost::future<ItemList> lookup(
        string const& parent_id, string const& name,
        Context const& ctx) override;
    boost::future<Item> metadata(
        string const& item_id, Context const& ctx) override;
    boost::future<Item> create_folder(
        string const& parent_id, string const& name,
        Context const& ctx) override;

    boost::future<unique_ptr<UploadJob>> create_file(
        string const& parent_id, string const& name,
        int64_t size, string const& content_type, bool allow_overwrite,
        Context const& ctx) override;
    boost::future<unique_ptr<UploadJob>> update(
        string const& item_id, int64_t size, string const& old_etag,
        Context const& ctx) override;

    boost::future<unique_ptr<DownloadJob>> download(
        string const& item_id, Context const& ctx) override;

    boost::future<void> delete_item(
        string const& item_id, Context const& ctx) override;
    boost::future<Item> move(
        string const& item_id, string const& new_parent_id,
        string const& new_name, Context const& ctx) override;
    boost::future<Item> copy(
        string const& item_id, string const& new_parent_id,
        string const& new_name, Context const& ctx) override;
};
