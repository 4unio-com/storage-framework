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

#include <unity/storage/provider/testing/TestServer.h>
#include <unity/storage/provider/internal/TestServerImpl.h>
#include <unity/storage/provider/ProviderBase.h>

#include <OnlineAccounts/Account>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{
namespace testing
{

TestServer::TestServer(unique_ptr<ProviderBase>&& provider,
                       OnlineAccounts::Account* account,
                       QDBusConnection const& connection,
                       string const& object_path)
    : p_(new internal::TestServerImpl(move(provider), account,
                                      connection, object_path))
{
}

TestServer::~TestServer() = default;

QDBusConnection const& TestServer::connection() const
{
    return p_->connection();
}

string const& TestServer::object_path() const
{
    return p_->object_path();
}

}
}
}
}
