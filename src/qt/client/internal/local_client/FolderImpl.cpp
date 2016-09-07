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

#include <unity/storage/qt/client/internal/local_client/FolderImpl.h>

#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/File.h>
#include <unity/storage/qt/client/Folder.h>
#include <unity/storage/qt/client/Uploader.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/local_client/FileImpl.h>
#include <unity/storage/qt/client/internal/local_client/storage_exception.h>
#include <unity/storage/qt/client/internal/local_client/tmpfile_prefix.h>
#include <unity/storage/qt/client/internal/local_client/UploaderImpl.h>

#include <boost/algorithm/string/predicate.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QtConcurrent>
#pragma GCC diagnostic pop

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

FolderImpl::FolderImpl(QString const& identity)
    : ItemBase(identity, ItemType::folder)
    , FolderBase(identity, ItemType::folder)
    , ItemImpl(identity, ItemType::folder)
{
}

FolderImpl::FolderImpl(QString const& identity, ItemType type)
    : ItemBase(identity, type)
    , FolderBase(identity, type)
    , ItemImpl(identity, type)
{
}

QString FolderImpl::name() const
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    throw_if_destroyed("Item::name()");
    return name_;
}

QFuture<QVector<Item::SPtr>> FolderImpl::list() const
{
    try
    {
        throw_if_destroyed("Folder::list()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<QVector<Item::SPtr>>(e);
    }

    auto This = dynamic_pointer_cast<FolderImpl const>(shared_from_this());  // Keep this folder alive while the lambda is alive.
    auto list = [This]()
    {
        lock_guard<decltype(mutex_)> guard(This->mutex_);

        This->throw_if_destroyed("Folder::list()");
        try
        {
            using namespace boost::filesystem;

            auto root = This->root_.lock();
            QVector<Item::SPtr> results;
            for (directory_iterator it(This->native_identity().toStdString()); it != directory_iterator(); ++it)
            {
                auto dirent = *it;
                file_status s = dirent.status();
                if (is_reserved_path(dirent.path()))
                {
                    continue;  // Hide temp files that we create during copy() and move().
                }
                QString path = QString::fromStdString(dirent.path().native());
                if (is_directory(s))
                {
                    results.append(make_folder(path, root));
                }
                else if (is_regular_file(s))
                {
                    results.append(FileImpl::make_file(path, root));
                }
                else
                {
                    // Ignore everything that's not a directory or file.
                }
            }
            return results;
        }
        catch (std::exception const&)
        {
            throw_storage_exception("Folder::list()", current_exception());
        }
    };
    return QtConcurrent::run(list);
}

QFuture<QVector<Item::SPtr>> FolderImpl::lookup(QString const& name) const
{
    try
    {
        throw_if_destroyed("Folder::lookup()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<QVector<Item::SPtr>>(e);
    }

    auto This = dynamic_pointer_cast<FolderImpl const>(shared_from_this());  // Keep this folder alive while the lambda is alive.
    auto lookup = [This, name]() -> QVector<Item::SPtr>
    {
        lock_guard<decltype(mutex_)> guard(This->mutex_);

        This->throw_if_destroyed("Folder::lookup()");  // LCOV_EXCL_LINE
        try
        {
            using namespace boost::filesystem;

            auto root = This->root_.lock();
            path p = This->native_identity().toStdString();
            auto sanitized_name = sanitize(name, "Folder::lookup()");
            if (is_reserved_path(sanitized_name))
            {
                throw NotExistsException("Folder::lookup(): no such item: \"" + name + "\"", name);
            }
            p /= sanitized_name;
            file_status s = status(p);
            if (is_directory(s))
            {
                QVector<Item::SPtr> v;
                v.append(make_folder(QString::fromStdString(p.native()), root));
                return v;
            }
            if (is_regular_file(s))
            {
                QVector<Item::SPtr> v;
                v.append(FileImpl::make_file(QString::fromStdString(p.native()), root));
                return v;
            }
            throw NotExistsException("Folder::lookup(): no such item: \"" + name + "\"", name);
        }
        catch (std::exception const&)
        {
            throw_storage_exception("Folder::lookup()", current_exception());
        }
    };
    return QtConcurrent::run(lookup);
}

QFuture<Folder::SPtr> FolderImpl::create_folder(QString const& name)
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Folder::create_folder()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<Folder::SPtr>(e);
    }

    try
    {
        using namespace boost::filesystem;

        path p = native_identity().toStdString();
        auto sanitized_name = sanitize(name, "Folder::create_folder()");
        if (is_reserved_path(sanitized_name))
        {
            QString msg = "Folder::create_folder(): names beginning with \"" + QString(TMPFILE_PREFIX) + "\" are reserved";
            throw InvalidArgumentException(msg);
        }
        p /= sanitized_name;
        if (exists(p))
        {
            QString msg = "Folder::create_folder(): item with name \"" + name + "\" exists already";
            throw ExistsException(msg, native_identity() + "/" + name, name);
        }
        create_directory(p);
        return make_ready_future(make_folder(QString::fromStdString(p.native()), root_));
    }
    catch (std::exception const&)
    {
        return make_exceptional_future<Folder::SPtr>("Folder::create_folder()", current_exception());
    }
}

QFuture<shared_ptr<Uploader>> FolderImpl::create_file(QString const& name, int64_t size)
{
    lock_guard<decltype(mutex_)> guard(mutex_);

    try
    {
        throw_if_destroyed("Folder::create_file()");
    }
    catch (StorageException const& e)
    {
        return internal::make_exceptional_future<shared_ptr<Uploader>>(e);
    }
    if (size < 0)
    {
        QString msg = "Folder::create_file(): size must be >= 0";
        return internal::make_exceptional_future<shared_ptr<Uploader>>(InvalidArgumentException(msg));
    }

    try
    {
        using namespace boost::filesystem;

        path p = native_identity().toStdString();
        auto sanitized_name = sanitize(name, "Folder::create_file()");
        if (is_reserved_path(sanitized_name))
        {
            QString msg = "Folder::create_file(): names beginning with \"" + QString(TMPFILE_PREFIX) + "\" are reserved";
            throw InvalidArgumentException(msg);
        }
        p /= sanitized_name;
        if (exists(p))
        {
            QString msg = "Folder::create_file(): item with name \"" + name + "\" exists already";
            throw ExistsException(msg, native_identity() + "/" + name, name);
        }
        auto impl = new UploaderImpl(shared_ptr<File>(),
                                     size,
                                     QString::fromStdString(p.native()),
                                     ConflictPolicy::error_if_conflict,
                                     root_);
        Uploader::SPtr uploader(new Uploader(impl));
        return make_ready_future(uploader);
    }
    catch (std::exception const&)
    {
        return make_exceptional_future<Uploader::SPtr>("Folder::create_file()", current_exception());
    }
}

Folder::SPtr FolderImpl::make_folder(QString const& identity, weak_ptr<Root> root)
{
    auto impl = new FolderImpl(identity);
    Folder::SPtr folder(new Folder(impl));
    impl->set_root(root);
    impl->set_public_instance(folder);
    return folder;
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
