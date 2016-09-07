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

#include <unity/storage/qt/client/internal/local_client/RootImpl.h>

#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/local_client/FileImpl.h>
#include <unity/storage/qt/client/internal/local_client/storage_exception.h>
#include <unity/storage/qt/client/Root.h>

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
namespace local_client
{

RootImpl::RootImpl(QString const& identity, weak_ptr<Account> const& account)
    : ItemBase(identity, ItemType::root)
    , FolderBase(identity, ItemType::root)
    , RootBase(identity, account)
    , ItemImpl(identity, ItemType::root)
    , FolderImpl(identity, ItemType::root)
{
    using namespace boost::filesystem;

    path id_path = path(identity.toStdString());
    if (!id_path.is_absolute())
    {
        QString msg = QString("Root: root path \"") + identity + "\" must be absolute";
        throw InvalidArgumentException(msg);
    }
    path can_path = canonical(id_path);
    auto id_len = std::distance(id_path.begin(), id_path.end());
    auto can_len = std::distance(can_path.begin(), can_path.end());
    if (id_len != can_len)
    {
        // identity denotes a weird path that we won't trust because
        // it might contain ".." or similar.
        QString msg = QString("Root: root path \"") + identity + "\" cannot contain \".\" or \"..\" components";
        throw InvalidArgumentException(msg);
    }
    assert(account.lock());
}

QString RootImpl::name() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    throw_if_destroyed("Item::name()");
    return "";
}

QFuture<QVector<Folder::SPtr>> RootImpl::parents() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Item::parents()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<QVector<Folder::SPtr>>(e);
    }
    return make_ready_future(QVector<Folder::SPtr>());  // For the root, we return an empty vector.
}

QVector<QString> RootImpl::parent_ids() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    throw_if_destroyed("Item::parent_ids()");
    return QVector<QString>();  // For the root, we return an empty vector.
}

QFuture<void> RootImpl::delete_item()
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Item::delete_item()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future(e);
    }
    // Cannot delete root.
    return internal::make_exceptional_future(LogicException("Item::delete_item(): cannot delete root folder"));
}

QFuture<int64_t> RootImpl::free_space_bytes() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Root::free_space_bytes()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<int64_t>(e);
    }

    using namespace boost::filesystem;

    try
    {
        space_info si = space(identity_.toStdString());
        return make_ready_future<int64_t>(si.available);
    }
    // LCOV_EXCL_START
    catch (std::exception const&)
    {
        return make_exceptional_future<int64_t>(QString("Root::free_space_bytes()"), current_exception());
    }
    // LCOV_EXCL_STOP
}

QFuture<int64_t> RootImpl::used_space_bytes() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Root::used_space_bytes()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<int64_t>(e);
    }

    using namespace boost::filesystem;

    try
    {
        space_info si = space(identity_.toStdString());
        return make_ready_future<int64_t>(si.capacity - si.available);
    }
    // LCOV_EXCL_START
    catch (std::exception const&)
    {
        return make_exceptional_future<int64_t>(QString("Root::used_space_bytes()"), current_exception());
    }
    // LCOV_EXCL_STOP
}

QFuture<Item::SPtr> RootImpl::get(QString native_identity) const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Root::get()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<Item::SPtr>(e);
    }

    auto root = get_root();
    if (!root)
    {
        return internal::make_exceptional_future<Item::SPtr>(RuntimeDestroyedException("Root::get()"));
    }

    using namespace boost::filesystem;

    QFutureInterface<Item::SPtr> qf;
    try
    {
        path id_path = native_identity.toStdString();
        if (!id_path.is_absolute())
        {
            QString msg = "Root::get(): identity \"" + native_identity + "\" must be an absolute path";
            throw InvalidArgumentException(msg);
        }

        // Make sure that native_identity is contained in or equal to the root path.
        id_path = canonical(id_path);
        auto root_path = path(root->native_identity().toStdString());
        auto id_len = std::distance(id_path.begin(), id_path.end());
        auto root_len = std::distance(root_path.begin(), root_path.end());
        if (id_len < root_len || !std::equal(root_path.begin(), root_path.end(), id_path.begin()))
        {
            // Too few components, or wrong path prefix. Therefore, native_identity can't
            // possibly point at something below the root.
            QString msg = QString("Root::get(): identity \"") + native_identity + "\" points outside the root folder";
            throw InvalidArgumentException(msg);
        }

        // Don't allow reserved files to be found.
        if (is_reserved_path(id_path))
        {
            QString msg = "Root::get(): no such item: \"" + native_identity + "\"";
            throw NotExistsException(msg, native_identity);
        }

        file_status s = status(id_path);
        QString path = QString::fromStdString(id_path.native());
        if (is_directory(s))
        {
            if (id_path == root_path)
            {
                return make_ready_future<Item::SPtr>(make_root(path, account()));
            }
            return make_ready_future<Item::SPtr>(make_folder(path, root));
        }
        if (is_regular_file(s))
        {
            return make_ready_future<Item::SPtr>(FileImpl::make_file(path, root));
        }
        QString msg = "Root::get(): no such item: \"" + native_identity + "\"";
        throw NotExistsException(msg, native_identity);
    }
    catch (std::exception const&)
    {
        return make_exceptional_future<Item::SPtr>(QString("Root::get()"), current_exception(), native_identity);
    }
}

Root::SPtr RootImpl::make_root(QString const& identity, std::weak_ptr<Account> const& account)
{
    assert(account.lock());

    auto impl = new RootImpl(identity, account);
    Root::SPtr root(new Root(impl));
    impl->set_root(root);
    impl->set_public_instance(root);
    return root;
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
