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

#include <unity/storage/qt/client/Root.h>

#include <unity/storage/qt/client/internal/RootBase.h>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

using namespace internal;
using namespace std;

Root::Root(RootBase* p)
    : Folder(p)
{
}

Root::~Root() = default;

shared_ptr<Account> Root::account() const
{
    return dynamic_cast<RootBase*>(p_.get())->account();
}

QFuture<int64_t> Root::free_space_bytes() const
{
    return dynamic_cast<RootBase*>(p_.get())->free_space_bytes();
}

QFuture<int64_t> Root::used_space_bytes() const
{
    return dynamic_cast<RootBase*>(p_.get())->used_space_bytes();
}

QFuture<Item::SPtr> Root::get(QString native_identity) const
{
    return dynamic_cast<RootBase*>(p_.get())->get(native_identity);
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
