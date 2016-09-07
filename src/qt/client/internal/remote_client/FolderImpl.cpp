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

#include <unity/storage/qt/client/internal/remote_client/FolderImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/qt/client/Folder.h>
#include <unity/storage/qt/client/internal/remote_client/FileImpl.h>
#include <unity/storage/qt/client/internal/remote_client/Handler.h>
#include <unity/storage/qt/client/internal/remote_client/UploaderImpl.h>
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

FolderImpl::FolderImpl(storage::internal::ItemMetadata const& md)
    : ItemBase(md.item_id, ItemType::folder)
    , FolderBase(md.item_id, ItemType::folder)
    , ItemImpl(md, ItemType::folder)
{
}

FolderImpl::FolderImpl(storage::internal::ItemMetadata const& md, ItemType type)
    : ItemBase(md.item_id, type)
    , FolderBase(md.item_id, type)
    , ItemImpl(md, type)
{
}

QFuture<QVector<shared_ptr<Item>>> FolderImpl::list() const
{
    try
    {
        throw_if_destroyed("Folder::list()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<QVector<shared_ptr<Item>>>(e);
    }

    auto prov = provider();
    auto reply = prov->List(md_.item_id, "");

    // Sorry for the mess, but we can't use auto for the lambda because it calls itself,
    // and the compiler can't deduce the type of the lambda while it's still parsing the lambda body.
    function<void(decltype(reply) const&, QFutureInterface<QVector<shared_ptr<Item>>>&)> process_reply
        = [this, prov, &process_reply](decltype(reply) const& reply, QFutureInterface<QVector<shared_ptr<Item>>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Folder::list()"));
            qf.reportFinished();
            return;
        }

        QVector<shared_ptr<Item>> items;
        auto metadata = reply.argumentAt<0>();
        for (auto const& md : metadata)
        {
            try
            {
                validate("Folder::list()", md);
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
                continue;
            }
            items.append(ItemImpl::make_item(md, root));
        }
        qf.reportResult(items, qf.resultCount());

        QString token = reply.argumentAt<1>();
        if (token.isEmpty())
        {
            qf.reportFinished();  // This was the last lot of results.
        }
        else
        {
            // Request next lot.
            auto next_reply = prov->List(md_.item_id, token);
            new Handler<QVector<shared_ptr<Item>>>(const_cast<FolderImpl*>(this), next_reply, process_reply);
        }
    };

    auto handler = new Handler<QVector<shared_ptr<Item>>>(const_cast<FolderImpl*>(this), reply, process_reply);
    return handler->future();
}

QFuture<QVector<shared_ptr<Item>>> FolderImpl::lookup(QString const& name) const
{
    try
    {
        throw_if_destroyed("Folder::lookup()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<QVector<shared_ptr<Item>>>(e);
    }

    auto prov = provider();
    auto reply = prov->Lookup(md_.item_id, name);

    auto process_reply = [this, name](decltype(reply) const& reply, QFutureInterface<QVector<shared_ptr<Item>>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Folder::lookup()"));
            qf.reportFinished();
            return;
        }

        QVector<Item::SPtr> items;
        auto metadata = reply.value();
        for (auto const& md : metadata)
        {
            try
            {
                validate("Folder::lookup()", md);
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
                continue;
            }
            items.append(ItemImpl::make_item(md, root));
        }
        if (items.isEmpty())
        {
            qf.reportException(NotExistsException("Folder::lookup(): no such item: " + name, name));
            qf.reportFinished();
            return;
        }
        qf.reportResult(items);
        qf.reportFinished();
    };

    auto handler = new Handler<QVector<shared_ptr<Item>>>(const_cast<FolderImpl*>(this), reply, process_reply);
    return handler->future();
}

QFuture<shared_ptr<Folder>> FolderImpl::create_folder(QString const& name)
{
    try
    {
        throw_if_destroyed("Folder::create_folder()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<shared_ptr<Folder>>(e);
    }

    auto prov = provider();
    auto reply = prov->CreateFolder(md_.item_id, name);

    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<shared_ptr<Folder>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Folder::create_folder()"));
            qf.reportFinished();
            return;
        }

        shared_ptr<Item> item;
        auto md = reply.value();
        try
        {
            validate("Folder::create_folder()", md);
        }
        catch (StorageException const& e)
        {
            qf.reportException(e);
            qf.reportFinished();
            return;
        }
        if (md.type != ItemType::folder)
        {
            // TODO: log server error here
            QString msg = "File::create_folder(): impossible item type returned by server: "
                          + QString::number(int(md.type));
            qf.reportException(LocalCommsException(msg));
            qf.reportFinished();
            return;
        }
        qf.reportResult(FolderImpl::make_folder(md, root));
        qf.reportFinished();
    };

    auto handler = new Handler<shared_ptr<Folder>>(this, reply, process_reply);
    return handler->future();
}

QFuture<shared_ptr<Uploader>> FolderImpl::create_file(QString const& name, int64_t size)
{
    try
    {
        throw_if_destroyed("Folder::create_file()");
    }
    catch (StorageException const& e)
    {
        return make_exceptional_future<shared_ptr<Uploader>>(e);
    }
    if (size < 0)
    {
        QString msg = "Folder::create_file(): size must be >= 0";
        return make_exceptional_future<shared_ptr<Uploader>>(InvalidArgumentException(msg));
    }

    auto prov = provider();
    auto reply = prov->CreateFile(md_.item_id, name, size, "application/octet-stream", false);

    auto process_reply = [this, size](decltype(reply) const& reply, QFutureInterface<shared_ptr<Uploader>>& qf)
    {
        auto root = get_root();
        if (!root)
        {
            qf.reportException(RuntimeDestroyedException("Folder::create_file()"));
            qf.reportFinished();
            return;
        }

        auto upload_id = reply.argumentAt<0>();
        auto fd = reply.argumentAt<1>();
        auto uploader = UploaderImpl::make_uploader(upload_id, fd, size, "", root, provider());
        qf.reportResult(uploader);
        qf.reportFinished();
    };

    auto handler = new Handler<shared_ptr<Uploader>>(this, reply, process_reply);
    return handler->future();
}

shared_ptr<Folder> FolderImpl::make_folder(storage::internal::ItemMetadata const& md, weak_ptr<Root> root)
{
    assert(md.type == ItemType::folder);
    assert(root.lock());

    auto impl = new FolderImpl(md);
    shared_ptr<Folder> folder(new Folder(impl));
    impl->set_root(root);
    impl->set_public_instance(folder);
    return folder;
}

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
