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

#pragma once

#include <unity/storage/provider/testing/TestServer.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QDBusConnection>
#pragma GCC diagnostic pop

#include <memory>
#include <string>

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

class ProviderInterface;

class TestServerImpl
{
public:
    TestServerImpl(std::unique_ptr<ProviderBase>&& provider,
                   OnlineAccounts::Account* account,
                   QDBusConnection const& connection,
                   std::string const& object_path);
    ~TestServerImpl();

    QDBusConnection const& connection() const;
    std::string const& object_path() const;

private:
    QDBusConnection connection_;
    std::string const object_path_;

    std::unique_ptr<ProviderInterface> interface_;
};

}
}
}
}
