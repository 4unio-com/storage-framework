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

#include <unity/storage/common.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/ItemBase.h>
#include <unity/storage/qt/client/internal/local_client/boost_filesystem.h>

#include <mutex>

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

class MetadataImpl;

namespace local_client
{

class ItemImpl : public virtual ItemBase
{
public:
    ItemImpl(QString const& identity, ItemType type);
    virtual ~ItemImpl();

    virtual QString etag() const override;
    virtual QVariantMap metadata() const override;
    virtual QDateTime last_modified_time() const override;
    virtual QFuture<std::shared_ptr<Item>> copy(std::shared_ptr<Folder> const& new_parent, QString const& new_name) override;
    virtual QFuture<std::shared_ptr<Item>> move(std::shared_ptr<Folder> const& new_parent, QString const& new_name) override;
    virtual QFuture<QVector<std::shared_ptr<Folder>>> parents() const override;
    virtual QVector<QString> parent_ids() const override;
    virtual QFuture<void> delete_item() override;

    virtual QDateTime creation_time() const override;
    virtual MetadataMap native_metadata() const override;

    virtual bool equal_to(ItemBase const& other) const noexcept override;

    void set_timestamps() noexcept;
    bool has_conflict() const noexcept;

protected:
    static boost::filesystem::path sanitize(QString const& name, QString const& method);
    static bool is_reserved_path(boost::filesystem::path const& path) noexcept;

    QString name_;
    QString etag_;
    QDateTime modified_time_;
    QVariantMap metadata_;
    std::recursive_mutex mutable mutex_;

private:
    static void copy_recursively(boost::filesystem::path const& source, boost::filesystem::path const& target);
};

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
