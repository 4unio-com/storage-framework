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

#include <unity/storage/qt/client/internal/local_client/ItemImpl.h>

#include <unity/storage/internal/safe_strerror.h>
#include <unity/storage/qt/client/Account.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/local_client/AccountImpl.h>
#include <unity/storage/qt/client/internal/local_client/FileImpl.h>
#include <unity/storage/qt/client/internal/local_client/RootImpl.h>
#include <unity/storage/qt/client/internal/local_client/tmpfile-prefix.h>

#include <boost/algorithm/string/predicate.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QtConcurrent>
#pragma GCC diagnostic pop

#include <sys/stat.h>

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
namespace local_client
{

ItemImpl::ItemImpl(QString const& identity, ItemType type)
    : ItemBase(identity, type)
    , deleted_(false)
{
    assert(!identity.isEmpty());
    auto path = boost::filesystem::canonical(identity.toStdString());
    name_ = QString::fromStdString(path.filename().native());
    set_timestamps();
}

ItemImpl::~ItemImpl() = default;

QString ItemImpl::name() const
{
    lock_guard<mutex> guard(mutex_);

    if (deleted_)
    {
        throw deleted_ex("Item::name()");
    }
    return name_;
}

QString ItemImpl::etag() const
{
    lock_guard<mutex> guard(mutex_);

    if (deleted_)
    {
        throw deleted_ex("Item::etag()");
    }
    return etag_;
}

QVariantMap ItemImpl::metadata() const
{
    lock_guard<mutex> guard(mutex_);

    if (deleted_)
    {
        throw deleted_ex("Item::metadata()");
    }
    return metadata_;
}

QDateTime ItemImpl::last_modified_time() const
{
    lock_guard<mutex> guard(mutex_);

    if (deleted_)
    {
        throw deleted_ex("Item::last_modified_time()");
    }
    return modified_time_;
}

namespace
{

using namespace boost::filesystem;

void copy_recursively(path const& source, path const& target)
{
    auto s = status(source);
    if (is_regular_file(s))
    {
        copy_file(source, target);
        return;
    }
    else if (is_directory(s))
    {
        copy_directory(source, target);  // Poorly named in boost; this creates the target dir without recursion
        for (directory_iterator it(source); it != directory_iterator(); ++it)
        {
            path source_entry = it->path();
            path target_entry = target;
            target_entry /= source_entry.filename();
            copy_recursively(source_entry, target_entry);
        }
    }
    else
    {
        // Ignore everything that's not a directory or file.
    }
}

}  // namespace

QFuture<shared_ptr<Item>> ItemImpl::copy(shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    auto This = dynamic_pointer_cast<ItemImpl>(shared_from_this());  // Keep this item alive while the lambda is alive.
    auto copy = [This, new_parent, new_name]() -> Item::SPtr
    {
        auto new_parent_impl = dynamic_pointer_cast<FolderImpl>(new_parent->p_);

        lock(This->mutex_, new_parent_impl->mutex_);
        lock_guard<mutex> this_guard(This->mutex_, std::adopt_lock);
        lock_guard<mutex> other_guard(new_parent_impl->mutex_, adopt_lock);

        if (This->deleted_ || new_parent_impl->deleted_)
        {
            throw This->deleted_ex("Item::copy");
        }

        if (This->root()->account() != new_parent->root()->account())
        {
            // Can't do cross-account copy.
            QString msg = QString("Item::copy(): Source (") + This->name_ + ") and target ("
                          + new_name + ") must belong to the same account";
            throw LogicException(msg);
        }

        try
        {
            using namespace boost::filesystem;

            path source_path = This->native_identity().toStdString();
            path parent_path = new_parent->native_identity().toStdString();
            path target_path = parent_path;
            path sanitized_name = sanitize(new_name, "Item::copy()");
            target_path /= sanitized_name;
            if (is_reserved_path(target_path))
            {
                QString msg = "Item::copy(): names beginning with " + QString(TMPFILE_PREFIX) + " are reserved";
                throw InvalidArgumentException(msg);
            }

            if (This->type_ == ItemType::file)
            {
                copy_file(source_path, target_path);
                return FileImpl::make_file(QString::fromStdString(target_path.native()), new_parent_impl->root_);
            }

            if (exists(target_path))
            {
                QString msg = "Item::copy(): item with name \"" + new_name + "\" exists already";
                throw ExistsException(msg, This->identity_, This->name_);
            }

            // For recursive copy, we create a temporary directory in lieu of target_path and recursively copy
            // everything into the temporary directory. This ensures that we don't invalidate directory iterators
            // by creating things while we are iterating, potentially getting trapped in an infinite loop.
            path tmp_path = canonical(parent_path);
            tmp_path /= unique_path(TMPFILE_PREFIX "%%%%-%%%%-%%%%-%%%%");
            create_directories(tmp_path);
            for (directory_iterator it(source_path); it != directory_iterator(); ++it)
            {
                if (tmp_path.compare(canonical(it->path())) == 0)
                {
                    continue;  // Don't recurse into the temporary directory
                }
                file_status s = it->status();
                if (is_directory(s) || is_regular_file(s))
                {
                    path source_entry = it->path();
                    path target_entry = tmp_path;
                    target_entry /= source_entry.filename();
                    copy_recursively(source_entry, target_entry);
                }
            }
            rename(tmp_path, target_path);
            return FolderImpl::make_folder(QString::fromStdString(target_path.native()), new_parent_impl->root_);
        }
        catch (boost::filesystem::filesystem_error const& e)
        {
            throw ResourceException(QString("Item::copy(): ") + e.what(), e.code().value());
        }
        catch (std::exception const& e)
        {
            throw ResourceException(QString("Item::copy(): ") + e.what(), errno);
        }
    };
    return QtConcurrent::run(copy);
}

QFuture<shared_ptr<Item>> ItemImpl::move(shared_ptr<Folder> const& new_parent, QString const& new_name)
{
    auto This = dynamic_pointer_cast<ItemImpl>(shared_from_this());  // Keep this item alive while the lambda is alive.
    auto move = [This, new_parent, new_name]() -> Item::SPtr
    {
        auto new_parent_impl = dynamic_pointer_cast<FolderImpl>(new_parent->p_);

        lock(This->mutex_, new_parent_impl->mutex_);
        lock_guard<mutex> this_guard(This->mutex_, std::adopt_lock);
        lock_guard<mutex> other_guard(new_parent_impl->mutex_, adopt_lock);

        if (This->deleted_ || new_parent_impl->deleted_)
        {
            throw This->deleted_ex("Item::move");
        }

        if (This->root()->account() != new_parent->root()->account())
        {
            // Can't do cross-account move.
            QString msg = QString("Item::move(): Source (") + This->name_ + ") and target ("
                          + new_name + ") must belong to the same account";
            throw LogicException(msg);
        }
        if (This->type_ == ItemType::root)
        {
            // Can't move a root.
            throw LogicException("Item::move(): Cannot move root folder");
        }

        try
        {
            using namespace boost::filesystem;

            path target_path = new_parent->native_identity().toStdString();
            target_path /= sanitize(new_name, "Item::move()");
            if (exists(target_path))
            {
                QString msg = "Item::move(): item with name \"" + new_name + "\" exists already";
                throw ExistsException(msg, This->identity_, This->name_);
            }
            if (is_reserved_path(target_path))
            {
                QString msg = "Item::move(): names beginning with " + QString(TMPFILE_PREFIX) + " are reserved";
                throw InvalidArgumentException(msg);
            }
            rename(This->native_identity().toStdString(), target_path);
            This->deleted_ = true;
            if (This->type_ == ItemType::folder)
            {
                return FolderImpl::make_folder(QString::fromStdString(target_path.native()), new_parent_impl->root_);
            }
            return FileImpl::make_file(QString::fromStdString(target_path.native()), new_parent_impl->root_);
        }
        catch (boost::filesystem::filesystem_error const& e)
        {
            throw ResourceException(QString("Item::move(): ") + e.what(), e.code().value());
        }
        catch (std::exception const& e)
        {
            throw ResourceException(QString("Item::move(): ") + e.what(), errno);
        }
    };
    return QtConcurrent::run(move);
}

QFuture<QVector<Folder::SPtr>> ItemImpl::parents() const
{
    lock_guard<mutex> guard(mutex_);

    QFutureInterface<QVector<Folder::SPtr>> qf;
    if (deleted_)
    {
        return make_exceptional_future<QVector<Folder::SPtr>>(deleted_ex("Item::parents()"));
    }

    Root::SPtr root = root_.lock();
    if (!root)
    {
        return make_exceptional_future<QVector<Folder::SPtr>>(RuntimeDestroyedException("Item::parents()"));
    }

    using namespace boost::filesystem;

    // We do this synchronously because we don't need to hit the file system.
    path p = native_identity().toStdString();
    QString parent_path = QString::fromStdString(p.parent_path().native());

    QVector<Folder::SPtr> results;
    if (parent_path != root->native_identity())
    {
        results.append(FolderImpl::make_folder(parent_path, root_));
    }
    else
    {
        results.append(root);
    }
    return make_ready_future(results);
}

QVector<QString> ItemImpl::parent_ids() const
{
    lock_guard<mutex> guard(mutex_);

    if (deleted_)
    {
        throw deleted_ex("Item::parent_ids()");
    }

    using namespace boost::filesystem;

    // We do this synchronously because we don't need to hit the file system.
    path p = native_identity().toStdString();
    QString parent_path = QString::fromStdString(p.parent_path().native());

    QVector<QString> results;
    results.append(parent_path);
    return results;
}

QFuture<void> ItemImpl::delete_item()
{
    auto This = dynamic_pointer_cast<ItemImpl>(shared_from_this());  // Keep this item alive while the lambda is alive.
    auto destroy = [This]()
    {
        lock_guard<mutex> guard(This->mutex_);

        if (This->deleted_)
        {
            throw This->deleted_ex("Item::delete_item()");
        }

        try
        {
            boost::filesystem::remove_all(This->native_identity().toStdString());
            This->deleted_ = true;
        }
        catch (boost::filesystem::filesystem_error const& e)
        {
            throw ResourceException(QString("Item::delete_item(): ") + e.what(), e.code().value());
        }
        catch (std::exception const& e)
        {
            throw ResourceException(QString("Item::delete_item(): ") + e.what(), errno);
        }
    };
    return QtConcurrent::run(destroy);
}

bool ItemImpl::equal_to(ItemBase const& other) const noexcept
{
    auto other_impl = dynamic_cast<ItemImpl const*>(&other);
    assert(other_impl);
    if (this == other_impl)
    {
        return true;
    }

    lock(mutex_, other_impl->mutex_);
    lock_guard<mutex> this_guard(mutex_, std::adopt_lock);
    lock_guard<mutex> other_guard(other_impl->mutex_, adopt_lock);

    if (deleted_ || other_impl->deleted_)
    {
        return false;
    }
    return identity_ == other_impl->identity_;
}

void ItemImpl::set_timestamps() noexcept
{
    lock_guard<mutex> guard(mutex_);

    string id = identity_.toStdString();
    // Use nano-second resolution for the ETag, if the file system supports it.
    struct stat st;
    if (stat(id.c_str(), &st) == -1)
    {
        // TODO: log this error
        modified_time_ = QDateTime::fromTime_t(0);
        etag_ = "";
    }
    modified_time_ = QDateTime::fromMSecsSinceEpoch(int64_t(st.st_mtim.tv_sec) * 1000 + st.st_mtim.tv_nsec / 1000000);
    etag_ = QString::number(int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec);
}

bool ItemImpl::has_conflict() const noexcept
{
    lock_guard<mutex> guard(mutex_);

    string id = identity_.toStdString();
    struct stat st;
    if (stat(id.c_str(), &st) == -1)
    {
        // TODO: log this error
        return true;
    }
    auto new_etag = QString::number(int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec);
    return etag_ != new_etag;
}

unique_lock<mutex> ItemImpl::get_lock()
{
    return unique_lock<mutex>(mutex_);
}

// Throw if name contains more than one path component.
// Otherwise, return the relative path for the name.
// This is to make sure that calling, say, create_file()
// with a name such as "../../whatever" cannot lead
// outside the root.

boost::filesystem::path ItemImpl::sanitize(QString const& name, QString const& method)
{
    using namespace boost::filesystem;

    path p = name.toStdString();
    if (!p.parent_path().empty())
    {
        // name contains more than one component.
        QString msg = method + ": name \"" + name + "\" contains more than one path component";
        throw InvalidArgumentException(msg);
    }
    path filename = p.filename();
    if (filename.empty() || filename == "." || filename == "..")
    {
        // Not an allowable file name.
        QString msg = method + ": invalid name: \"" + name + "\"";
        throw InvalidArgumentException(msg);
    }
    return p;
}

// Return true if the name uses the temp file prefix.

bool ItemImpl::is_reserved_path(boost::filesystem::path const& path) noexcept
{
    string filename = path.filename().native();
    return boost::starts_with(filename, TMPFILE_PREFIX);
}

DeletedException ItemImpl::deleted_ex(QString const& method) const noexcept
{
    QString msg = method + ": " + identity_ + " was deleted previously";
    return DeletedException(msg, identity_, name_);
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
