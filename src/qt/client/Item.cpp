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

#include <unity/storage/qt/client/Item.h>

#include <unity/storage/qt/client/internal/ItemBase.h>

#include <cassert>

using namespace std;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

Item::Item(internal::ItemBase* p)
    : p_(p)
{
    assert(p != nullptr);
}

Item::~Item() = default;

QString Item::native_identity() const
{
    return p_->native_identity();
}

QString Item::name() const
{
    return p_->name();
}

shared_ptr<Root> Item::root() const
{
    return p_->root();
}

ItemType Item::type() const
{
    return p_->type();
}

QString Item::etag() const
{
    return p_->etag();
}

QVariantMap Item::metadata() const
{
    return p_->metadata();
}

QDateTime Item::last_modified_time() const
{
    return p_->last_modified_time();
}

QFuture<QVector<std::shared_ptr<Folder>>> Item::parents() const
{
    return p_->parents();
}

QVector<QString> Item::parent_ids() const
{
    return p_->parent_ids();
}

QFuture<Item::SPtr> Item::copy(std::shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    return p_->copy(new_parent, new_name);
}
QFuture<Item::SPtr> Item::move(std::shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    return p_->move(new_parent, new_name);
}

QFuture<void> Item::delete_item()
{
    return p_->delete_item();
}

QDateTime Item::creation_time() const
{
    return p_->creation_time();
}

MetadataMap Item::native_metadata() const
{
    return p_->native_metadata();
}

bool Item::equal_to(Item::SPtr const& other) const noexcept
{
    return p_->equal_to(*other->p_);
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
