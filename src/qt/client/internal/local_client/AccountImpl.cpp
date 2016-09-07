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

#include <unity/storage/qt/client/internal/local_client/AccountImpl.h>

#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/local_client/RootImpl.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <glib.h>
#pragma GCC diagnostic pop

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

namespace
{

// Return ${STORAGE_FRAMEWORK_ROOT}/storage-framework. If STORAGE_FRAMEWORK_ROOT
// is not set, return ${XDG_DATA_HOME}/storage-framework.
// ${STORAGE_FRAMEWORK_ROOT} or ${XDG_DATA_HOME} must exist and be a directory.
// If the storage-framework underneath that data directory does not exist, it is created.

string get_data_dir()
{
    char const* dir = getenv("STORAGE_FRAMEWORK_ROOT");
    if (!dir || *dir == '\0')
    {
        dir = g_get_user_data_dir();  // Never fails.
    }

    boost::system::error_code ec;

    // The directory must exist.
    bool is_dir = boost::filesystem::is_directory(dir, ec);
    if (ec)
    {
        QString msg = "Account::roots(): Cannot stat " + QString(dir) + ": " + QString::fromStdString(ec.message());
        throw ResourceException(msg, errno);
    }
    if (!is_dir)
    {
        QString msg = "Account::roots(): Environment variable STORAGE_FRAMEWORK_ROOT must denote a directory";
        throw InvalidArgumentException(msg);
    }

    // Create the storage-framework directory if it doesn't exist yet.
    string data_dir(dir);
    data_dir += "/storage-framework";
    if (!boost::filesystem::exists(data_dir))
    {
        boost::filesystem::create_directories(data_dir, ec);
        if (ec)
        {
            QString msg = "Account::roots(): Cannot create " + QString(dir) + ": "
                          + QString::fromStdString(ec.message());
            throw ResourceException(msg, ec.value());
        }
    }
    return data_dir;
}

}  // namespace

AccountImpl::AccountImpl(weak_ptr<Runtime> const& runtime,
                         QString const& owner,
                         QString const& owner_id,
                         QString const& description)
    : AccountBase(runtime)
    , owner_(owner)
    , owner_id_(owner_id)
    , description_(description)
{
    assert(!owner.isEmpty());
    assert(!owner_id.isEmpty());
    assert(!description.isEmpty());
}

QString AccountImpl::owner() const
{
    runtime();  // Throws RuntimeDestroyedException if runtime was destroyed.
    return owner_;
}

QString AccountImpl::owner_id() const
{
    runtime();  // Throws RuntimeDestroyedException if runtime was destroyed.
    return owner_id_;
}

QString AccountImpl::description() const
{
    runtime();  // Throws RuntimeDestroyedException if runtime was destroyed.
    return description_;
}

QFuture<QVector<Root::SPtr>> AccountImpl::roots()
{
    try
    {
        runtime();  // Throws RuntimeDestroyedException if runtime was destroyed.
    }
    catch (RuntimeDestroyedException const& e)
    {
        return make_exceptional_future<QVector<Root::SPtr>>(e);
    }

    if (!roots_.isEmpty())
    {
        return make_ready_future(roots_);
    }

    // Create the root on first access.

    using namespace boost::filesystem;

    auto rpath = canonical(get_data_dir()).native();
    auto root = RootImpl::make_root(QString::fromStdString(rpath), public_instance_);
    roots_.append(root);
    return make_ready_future(roots_);
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
