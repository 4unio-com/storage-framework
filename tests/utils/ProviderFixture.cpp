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

#include "ProviderFixture.h"

#include <unity/storage/internal/dbus_error.h>

#include <QSignalSpy>

using namespace unity::storage::provider;
using namespace std;

namespace
{

const auto SERVICE_CONNECTION_NAME = QStringLiteral("service-session-bus");
const auto OBJECT_PATH = QStringLiteral("/provider");

}  // namespace

ProviderFixture::ProviderFixture()
{
    dbus_.reset(new DBusEnvironment);
    dbus_->start_services();
    service_connection_.reset(new QDBusConnection(QDBusConnection::connectToBus(dbus_->busAddress(),
                                                  SERVICE_CONNECTION_NAME)));
    account_manager_.reset(new OnlineAccounts::Manager("", *service_connection_));
}

ProviderFixture::~ProviderFixture()
{
    test_server_.reset();
    service_connection_.reset();
    QDBusConnection::disconnectFromBus(SERVICE_CONNECTION_NAME);
    dbus_.reset();
}

QDBusConnection const& ProviderFixture::connection() const
{
    return dbus_->connection();
}

void ProviderFixture::set_provider(unique_ptr<ProviderBase>&& provider)
{
    account_manager_->waitForReady();
    OnlineAccounts::Account* account = account_manager_->account(2, "oauth2-service");
    ASSERT_NE(nullptr, account);

    test_server_.reset(
        new unity::storage::provider::testing::TestServer(move(provider), account,
                                                          *service_connection_, OBJECT_PATH.toStdString()));
}

void ProviderFixture::wait_for(QDBusPendingCall const& call)
{
    QDBusPendingCallWatcher watcher(call);
    QSignalSpy spy(&watcher, &QDBusPendingCallWatcher::finished);
    ASSERT_TRUE(spy.wait());
}

QString ProviderFixture::bus_name() const
{
    return service_connection_->baseService();
}

QString ProviderFixture::object_path() const
{
    return OBJECT_PATH;
}
