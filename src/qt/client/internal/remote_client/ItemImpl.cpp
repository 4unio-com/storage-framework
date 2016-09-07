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

#include <unity/storage/qt/client/internal/remote_client/ItemImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/provider/metadata_keys.h>
#include <unity/storage/qt/client/Account.h>
#include <unity/storage/qt/client/internal/remote_client/AccountImpl.h>
#include <unity/storage/qt/client/internal/remote_client/FileImpl.h>
#include <unity/storage/qt/client/internal/remote_client/Handler.h>
#include <unity/storage/qt/client/internal/remote_client/RootImpl.h>
#include <unity/storage/qt/client/internal/remote_client/validate.h>

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
namespace internal
{
namespace remote_client
{

ItemImpl::ItemImpl(storage::internal::ItemMetadata const& md, ItemType type)
    : ItemBase(md.item_id, type)
    , md_(md)
{
}

QString ItemImpl::name() const
{
    throw_if_destroyed("Item::name()");
    return md_.name;
}

QString ItemImpl::etag() const
{
    throw_if_destroyed("Item::etag()");
    return md_.etag;
}

QVariantMap ItemImpl::metadata() const
{
    throw_if_destroyed("Item::metadata()");
    // TODO: need to agree on metadata representation
    return QVariantMap();
}

QDateTime ItemImpl::last_modified_time() const
{
    throw_if_destroyed("Item::last_modified_time()");
    return QDateTime::fromString(md_.metadata.value(provider::LAST_MODIFIED_TIME).toString(), Qt::ISODate);
}

QFuture<shared_ptr<Item>> ItemImpl::copy(shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    if (!new_parent)
    {
        QString msg = "Item::copy(): new_parent cannot be nullptr";
        return internal::make_exceptional_future<shared_ptr<Item>>(InvalidArgumentException(msg));
    }

    auto new_parent_impl = dynamic_pointer_cast<FolderImpl>(new_parent->p_);
    try
    {
        throw_if_destroyed("Item::copy()");
        new_parent_impl->throw_if_destroyed("Item::copy()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<shared_ptr<Item>>(e);
    }

    auto prov = provider();
    auto reply = prov->Copy(md_.item_id, new_parent->native_identity(), new_name);

    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<std::shared_ptr<Item>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Item::copy()"));
            qf.reportFinished();
            return;
        }

        auto md = reply.value();
        try
        {
            validate("Item::copy()", md);
        }
        catch (StorageException const& e)
        {
            qf.reportException(e);
            qf.reportFinished();
            return;
        }
        if (md.type == ItemType::root)
        {
            // TODO: log server error here
            QString msg = "File::create_folder(): impossible item type returned by server: "
                          + QString::number(int(md.type));
            qf.reportException(LocalCommsException(msg));
            qf.reportFinished();
            return;
        }
        qf.reportResult(ItemImpl::make_item(md, root));
        qf.reportFinished();
        return;
    };

    auto handler = new Handler<shared_ptr<Item>>(this, reply, process_reply);
    return handler->future();
}

QFuture<shared_ptr<Item>> ItemImpl::move(shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    if (!new_parent)
    {
        QString msg = "Item::move(): new_parent cannot be nullptr";
        return internal::make_exceptional_future<shared_ptr<Item>>(InvalidArgumentException(msg));
    }

    auto new_parent_impl = dynamic_pointer_cast<FolderImpl>(new_parent->p_);
    try
    {
        throw_if_destroyed("Item::move()");
        new_parent_impl->throw_if_destroyed("Item::move()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<shared_ptr<Item>>(e);
    }

    auto prov = provider();
    if (!prov)
    {
        return make_exceptional_future<shared_ptr<Item>>(RuntimeDestroyedException("Item::move()"));
    }
    auto reply = prov->Move(md_.item_id, new_parent->native_identity(), new_name);

    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<std::shared_ptr<Item>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Item::move()"));
            qf.reportFinished();
            return;
        }

        auto md = reply.value();
        try
        {
            validate("Item::move()", md);
        }
        catch (StorageException const& e)
        {
            qf.reportException(e);
            qf.reportFinished();
            return;
        }
        if (md.type == ItemType::root)
        {
            // TODO: log server error here
            QString msg = "Item::move(): impossible root item returned by server";
            qf.reportException(LocalCommsException(msg));
            qf.reportFinished();
            return;
        }
        qf.reportResult(ItemImpl::make_item(md, root));
        qf.reportFinished();
    };

    auto handler = new Handler<shared_ptr<Item>>(this, reply, process_reply);
    return handler->future();
}

QFuture<QVector<Folder::SPtr>> ItemImpl::parents() const
{
    try
    {
        throw_if_destroyed("Item::parents()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<QVector<Folder::SPtr>>(e);
    }
    // TODO, need different metadata representation, affects xml
    return QFuture<QVector<Folder::SPtr>>();
}

QVector<QString> ItemImpl::parent_ids() const
{
    throw_if_destroyed("Item::parent_ids()");
    // TODO, need different metadata representation, affects xml
    return md_.parent_ids;
}

QFuture<void> ItemImpl::delete_item()
{
    try
    {
        throw_if_destroyed("Item::delete_item()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future(e);
    }

    auto prov = provider();
    auto reply = prov->Delete(md_.item_id);

    auto process_reply = [this](decltype(reply) const&, QFutureInterface<void>& qf)
    {
        deleted_ = true;
        qf.reportFinished();
    };

    auto handler = new Handler<void>(this, reply, process_reply);
    return handler->future();
}

QDateTime ItemImpl::creation_time() const
{
    throw_if_destroyed("Item::creation_time()");
    return QDateTime::fromString(md_.metadata.value(provider::CREATION_TIME).toString(), Qt::ISODate);
}

MetadataMap ItemImpl::native_metadata() const
{
    throw_if_destroyed("Item::native_metadata()");
    // TODO: need to agree on metadata representation
    return MetadataMap();
}

bool ItemImpl::equal_to(ItemBase const& other) const noexcept
{
    auto other_impl = dynamic_cast<ItemImpl const*>(&other);
    assert(other_impl);
    if (this == other_impl)
    {
        return true;
    }
    if (deleted_ || other_impl->deleted_)
    {
        return false;
    }
    return identity_ == other_impl->identity_;
}

shared_ptr<ProviderInterface> ItemImpl::provider() const noexcept
{
    auto root = dynamic_pointer_cast<Root>(root_.lock());
    if (!root)
    {
        return nullptr;
    }
    auto root_impl = dynamic_pointer_cast<RootImpl>(root->p_);

    auto account = root_impl->account_.lock();
    if (!account)
    {
        return nullptr;
    }

    auto account_impl = dynamic_pointer_cast<AccountImpl>(account->p_);
    return account_impl->provider();
}

shared_ptr<Item> ItemImpl::make_item(storage::internal::ItemMetadata const& md, std::weak_ptr<Root> root)
{
    assert(md.type == ItemType::file || md.type == ItemType::folder);

    shared_ptr<Item> item;
    switch (md.type)
    {
        case ItemType::file:
        {
            item = FileImpl::make_file(md, root);
            break;
        }
        case ItemType::folder:
        {
            item = FolderImpl::make_folder(md, root);
            break;
        }
        default:
        {
            abort();  // LCOV_EXCL_LINE  // Impossible
        }
    }
    assert(item);
    return item;
}

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
