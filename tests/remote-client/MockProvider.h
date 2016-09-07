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

#include <unity/storage/provider/DownloadJob.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/UploadJob.h>

class MockProvider : public unity::storage::provider::ProviderBase
{
public:
    MockProvider();
    MockProvider(std::string const& cmd);

    boost::future<unity::storage::provider::ItemList> roots(unity::storage::provider::Context const& ctx) override;
    boost::future<std::tuple<unity::storage::provider::ItemList, std::string>> list(
        std::string const& item_id,  std::string const& page_token,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::ItemList> lookup(
        std::string const& parent_id,  std::string const& name,
        unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> metadata(
        std::string const& item_id, unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> create_folder(
        std::string const& parent_id,  std::string const& name,
        unity::storage::provider::Context const& ctx) override;

    boost::future<std::unique_ptr<unity::storage::provider::UploadJob>> create_file(
        std::string const& parent_id, std::string const& name,
        int64_t size, std::string const& content_type, bool allow_overwrite,
        unity::storage::provider::Context const& ctx) override;
    boost::future<std::unique_ptr<unity::storage::provider::UploadJob>> update(
        std::string const& item_id, int64_t size, std::string const& old_etag,
        unity::storage::provider::Context const& ctx) override;

    boost::future<std::unique_ptr<unity::storage::provider::DownloadJob>> download(
        std::string const& item_id, unity::storage::provider::Context const& ctx) override;

    boost::future<void> delete_item(
        std::string const& item_id, unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> move(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name, unity::storage::provider::Context const& ctx) override;
    boost::future<unity::storage::provider::Item> copy(
        std::string const& item_id, std::string const& new_parent_id,
        std::string const& new_name, unity::storage::provider::Context const& ctx) override;

private:
    std::string cmd_;
};

class MockUploadJob : public unity::storage::provider::UploadJob
{
public:
    using UploadJob::UploadJob;

    MockUploadJob();
    MockUploadJob(std::string const& cmd);

    boost::future<void> cancel() override;
    boost::future<unity::storage::provider::Item> finish() override;

private:
    std::string cmd_;
};

class MockDownloadJob : public unity::storage::provider::DownloadJob
{
public:
    using DownloadJob::DownloadJob;

    MockDownloadJob();
    MockDownloadJob(const std::string& cmd);

    boost::future<void> cancel() override;
    boost::future<void> finish() override;

private:
    std::string cmd_;
};
