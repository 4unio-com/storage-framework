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

#include <unity/storage/qt/client/internal/remote_client/RootImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/qt/client/internal/remote_client/FileImpl.h>
#include <unity/storage/qt/client/internal/remote_client/Handler.h>
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

RootImpl::RootImpl(storage::internal::ItemMetadata const& md, weak_ptr<Account> const& account)
    : ItemBase(md.item_id, ItemType::root)
    , FolderBase(md.item_id, ItemType::root)
    , RootBase(md.item_id, account)
    , ItemImpl(md, ItemType::root)
    , FolderImpl(md, ItemType::root)
{
}

QFuture<QVector<Folder::SPtr>> RootImpl::parents() const
{
    try
    {
        throw_if_destroyed("Root::parents()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<QVector<Folder::SPtr>>(e);
    }
    return make_ready_future(QVector<Folder::SPtr>());  // For the root, we return an empty vector.
}

QVector<QString> RootImpl::parent_ids() const
{
    throw_if_destroyed("Root::parent_ids()");
    return QVector<QString>();  // For the root, we return an empty vector.
}

QFuture<void> RootImpl::delete_item()
{
    try
    {
        throw_if_destroyed("Item::delete_item()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<QVector<Folder::SPtr>>(e);
    }
    // Cannot delete root.
    return make_exceptional_future(LogicException("Item::delete_item(): cannot delete root folder"));
}

QFuture<int64_t> RootImpl::free_space_bytes() const
{
    try
    {
        throw_if_destroyed("Root::free_space_bytes()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<int64_t>(e);
    }
    // TODO, need to refresh metadata here instead.
    return make_ready_future(int64_t(1));
}

QFuture<int64_t> RootImpl::used_space_bytes() const
{
    try
    {
        throw_if_destroyed("Root::used_space_bytes()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<int64_t>(e);
    }
    // TODO, need to refresh metadata here instead.
    return make_ready_future(int64_t(1));
}

QFuture<Item::SPtr> RootImpl::get(QString native_identity) const
{
    try
    {
        throw_if_destroyed("Root::get()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<Item::SPtr>(e);
    }

    auto prov = provider();
    auto reply = prov->Metadata(native_identity);

    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<Item::SPtr>& qf)
    {
        shared_ptr<Account> acc;
        try
        {
            acc = account();
        }
        catch (RuntimeDestroyedException const&)
        {
            qf.reportException(RuntimeDestroyedException("Root::get()"));
            qf.reportFinished();
            return;
        }

        auto md = reply.value();
        try
        {
            validate("Root::get()", md);
        }
        catch (StorageException const& e)
        {
            qf.reportException(e);
            qf.reportFinished();
            return;
        }
        Item::SPtr item;
        if (md.type == ItemType::root)
        {
            item = make_root(md, acc);
        }
        else
        {
            // acc owns the root, so the root weak_ptr is guaranteed to be lockable.
            item = ItemImpl::make_item(md, root_);
        }
        qf.reportResult(item);
        qf.reportFinished();
    };

    auto handler = new Handler<Item::SPtr>(const_cast<RootImpl*>(this), reply, process_reply);
    return handler->future();
}

Root::SPtr RootImpl::make_root(storage::internal::ItemMetadata const& md, weak_ptr<Account> const& account)
{
    assert(md.type == ItemType::root);
    assert(account.lock());

    auto impl = new RootImpl(md, account);
    Root::SPtr root(new Root(impl));
    impl->set_root(root);
    impl->set_public_instance(root);
    return root;
}

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
