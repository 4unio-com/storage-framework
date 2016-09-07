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

#include <unity/storage/qt/client/internal/RootBase.h>

#include <unity/storage/qt/client/Account.h>
#include <unity/storage/qt/client/Exceptions.h>

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

RootBase::RootBase(QString const& identity, weak_ptr<Account> const& account)
    : ItemBase(identity, ItemType::folder)
    , FolderBase(identity, ItemType::folder)
    , account_(account)
{
    assert(account.lock());
}

shared_ptr<Account> RootBase::account() const
{
    if (auto acc = account_.lock())
    {
        try
        {
            acc->runtime();
        }
        catch (RuntimeDestroyedException const&)
        {
            throw RuntimeDestroyedException("Root::account()");
        }
        return acc;
    }
    throw RuntimeDestroyedException("Root::account()");
}

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
