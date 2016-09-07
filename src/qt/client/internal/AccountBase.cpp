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

#include <unity/storage/qt/client/internal/AccountBase.h>

#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/RuntimeBase.h>
#include <unity/storage/qt/client/Runtime.h>

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

AccountBase::AccountBase(weak_ptr<Runtime> const& runtime)
    : runtime_(runtime)
{
    assert(runtime.lock());
}

shared_ptr<Runtime> AccountBase::runtime() const
{
    if (auto runtime = runtime_.lock())
    {
        auto runtime_base = runtime->p_;
        if (runtime_base->destroyed_)
        {
            throw RuntimeDestroyedException("Account::runtime()");
        }
        return runtime;
    }
    throw RuntimeDestroyedException("Account::runtime()");
}

void AccountBase::set_public_instance(weak_ptr<Account> const& p)
{
    public_instance_ = p;
}

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
