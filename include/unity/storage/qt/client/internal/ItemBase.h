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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QDateTime>
#include <QFuture>
#pragma GCC diagnostic pop
#include <QVariantMap>

#include <memory>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Folder;
class Item;
class Root;

typedef QMap<QString, QVariant> MetadataMap;

namespace internal
{

class ItemImpl;

class ItemBase : public std::enable_shared_from_this<ItemBase>
{
public:
    ItemBase(QString const& identity, ItemType type);
    virtual ~ItemBase();
    ItemBase(ItemBase const&) = delete;
    ItemBase& operator=(ItemBase const&) = delete;

    QString native_identity() const;
    ItemType type() const;
    std::shared_ptr<Root> root() const;

    virtual QString name() const = 0;
    virtual QString etag() const = 0;
    virtual QVariantMap metadata() const = 0;
    virtual QDateTime last_modified_time() const = 0;

    virtual QFuture<std::shared_ptr<Item>> copy(std::shared_ptr<Folder> const& new_parent, QString const& new_name) = 0;
    virtual QFuture<std::shared_ptr<Item>> move(std::shared_ptr<Folder> const& new_parent, QString const& new_name) = 0;
    virtual QFuture<QVector<std::shared_ptr<Folder>>> parents() const = 0;
    virtual QVector<QString> parent_ids() const = 0;
    virtual QFuture<void> delete_item() = 0;

    virtual QDateTime creation_time() const = 0;
    virtual MetadataMap native_metadata() const = 0;

    virtual bool equal_to(ItemBase const& other) const noexcept = 0;

    void set_root(std::weak_ptr<Root> p);
    void set_public_instance(std::weak_ptr<Item> p);

protected:
    std::shared_ptr<Root> get_root() const noexcept;
    void throw_if_destroyed(QString const& method) const;

    const QString identity_;
    const ItemType type_;
    std::weak_ptr<Root> root_;
    std::weak_ptr<Item> public_instance_;
    bool deleted_ = false;

    friend class ItemImpl;
};

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
