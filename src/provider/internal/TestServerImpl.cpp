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
 * Authors: James Henstridge <james.henstridge@canonical.com>
 */

#include <unity/storage/provider/internal/TestServerImpl.h>
#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/internal/AccountData.h>
#include <unity/storage/provider/internal/DBusPeerCache.h>
#include <unity/storage/provider/internal/ProviderInterface.h>
#include <unity/storage/provider/internal/dbusmarshal.h>
#include "provideradaptor.h"

#include <OnlineAccounts/Account>

#include <stdexcept>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

TestServerImpl::TestServerImpl(unique_ptr<ProviderBase>&& provider,
                               OnlineAccounts::Account* account,
                               QDBusConnection const& connection,
                               string const& object_path)
    : connection_(connection), object_path_(object_path)
{
    qDBusRegisterMetaType<Item>();
    qDBusRegisterMetaType<std::vector<Item>>();

    auto peer_cache = make_shared<DBusPeerCache>(connection_);
    auto account_data = make_shared<AccountData>(
        move(provider), peer_cache, connection_, account);
    interface_.reset(new ProviderInterface(account_data));
    new ProviderAdaptor(interface_.get());

    if (!connection_.registerObject(QString::fromStdString(object_path_),
                                    interface_.get()))
    {
        string msg = "Could not register provider on connection: " + connection_.lastError().message().toStdString();
        throw ResourceException(msg, int(connection_.lastError().type()));
    }
}

TestServerImpl::~TestServerImpl()
{
    connection_.unregisterObject(QString::fromStdString(object_path_));
}

QDBusConnection const& TestServerImpl::connection() const
{
    return connection_;
}

string const& TestServerImpl::object_path() const
{
    return object_path_;
}

}
}
}
}
