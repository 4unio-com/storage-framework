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

#include <unity/storage/qt/client/Runtime.h>

#include <unity/storage/qt/client/internal/RuntimeBase.h>

#include <QDBusConnection>

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

// Runtime::SPtr Runtime::create() is defined by local_client and remote_client, respectively.

Runtime::Runtime(internal::RuntimeBase* p)
    : p_(p)
{
    assert(p != nullptr);
}

Runtime::~Runtime()
{
    shutdown();
}

Runtime::SPtr Runtime::create()
{
    return Runtime::create(QDBusConnection::sessionBus());
}

void Runtime::shutdown()
{
    p_->shutdown();

}

QFuture<QVector<shared_ptr<Account>>> Runtime::accounts()
{
    return p_->accounts();
}

shared_ptr<Account> Runtime::make_test_account(QString const& bus_name,
                                               QString const& object_path)
{
    return p_->make_test_account(bus_name, object_path);
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
