/*
 * Copyright (C) 2017 Canonical Ltd
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

#include <unity/storage/provider/ProviderBase.h>

#include <boost/filesystem.hpp>

class LocalProvider : public unity::storage::provider::ProviderBase
{
public:
    LocalProvider();
    virtual ~LocalProvider();

    boost::future<unity::storage::provider::ItemList> roots(
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<std::tuple<unity::storage::provider::ItemList, std::string>> list(
        std::string const& item_id, std::string const& page_token,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::ItemList> lookup(
        std::string const& parent_id, std::string const& name,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> metadata(
        std::string const& item_id,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> create_folder(
        std::string const& parent_id, std::string const& name,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<std::unique_ptr<unity::storage::provider::UploadJob>> create_file(
        std::string const& parent_id, std::string const& name,
        int64_t size, std::string const& content_type, bool allow_overwrite,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<std::unique_ptr<unity::storage::provider::UploadJob>> update(
        std::string const& item_id, int64_t size,
        std::string const& old_etag,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<std::unique_ptr<unity::storage::provider::DownloadJob>> download(
        std::string const& item_id,
        std::string const& match_etag,
        unity::storage::provider::Context const& ctx) override;
    boost::future<void> delete_item(std::string const& item_id,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> move(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> copy(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name,
        std::vector<std::string> const& metadata_keys,
        unity::storage::provider::Context const& ctx) override;

    void throw_if_not_valid(std::string const& method, std::string const& id) const;
    unity::storage::provider::Item make_item(std::string const& method,
                                             boost::filesystem::path const& item_path,
                                             boost::filesystem::file_status const& st) const;

private:
    boost::filesystem::path const root_;
};
