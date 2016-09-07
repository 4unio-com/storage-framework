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

#include <unity/storage/internal/ItemMetadata.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/ItemBase.h>

#include <memory>

class ProviderInterface;

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
namespace remote_client
{

class DeleteHandler;

class ItemImpl : public virtual ItemBase, public virtual QObject
{
public:
    ItemImpl(storage::internal::ItemMetadata const& md, ItemType type);

    virtual QString name() const override;
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

    std::shared_ptr<ProviderInterface> provider() const noexcept;

    static std::shared_ptr<Item> make_item(storage::internal::ItemMetadata const& md, std::weak_ptr<Root> root);

protected:
    storage::internal::ItemMetadata md_;

    friend class DeleteHandler;
};

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
