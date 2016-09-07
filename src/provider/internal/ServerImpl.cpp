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

#include <unity/storage/provider/internal/ServerImpl.h>
#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/internal/AccountData.h>
#include <unity/storage/provider/internal/MainLoopExecutor.h>
#include <unity/storage/provider/internal/dbusmarshal.h>
#include "provideradaptor.h"

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

ServerImpl::ServerImpl(ServerBase* server, string const& bus_name, string const& account_service_id)
    : server_(server)
    , bus_name_(bus_name)
    , service_id_(account_service_id)
    , trace_message_handler_("storage_provider")
{
    qDBusRegisterMetaType<Item>();
    qDBusRegisterMetaType<std::vector<Item>>();
}

ServerImpl::~ServerImpl() = default;

void ServerImpl::init(int& argc, char **argv)
{
    app_.reset(new QCoreApplication(argc, argv));
    auto bus = QDBusConnection::sessionBus();
    dbus_peer_ = make_shared<DBusPeerCache>(bus);

#ifdef SF_SUPPORTS_EXECUTORS
    // Ensure the executor is instantiated in the main thread.
    MainLoopExecutor::instance();
#endif

    manager_.reset(new OnlineAccounts::Manager("", bus));
    connect(manager_.get(), &OnlineAccounts::Manager::ready,
                     this, &ServerImpl::account_manager_ready);
}

void ServerImpl::run()
{
    app_->exec();
}

void ServerImpl::account_manager_ready()
{
    auto bus = QDBusConnection::sessionBus();
    for (const auto& account : manager_->availableAccounts(QString::fromStdString(service_id_)))
    {
        qDebug() << "Found account" << account->id() << "for service" << account->serviceId();
        auto account_data = make_shared<AccountData>(
            server_->make_provider(), dbus_peer_, bus, account);
        unique_ptr<ProviderInterface> iface(
            new ProviderInterface(account_data));
        // this instance is managed by Qt's parent/child memory management
        new ProviderAdaptor(iface.get());

        bus.registerObject(QStringLiteral("/provider/%1").arg(account->id()), iface.get());
        interfaces_.emplace(account->id(), std::move(iface));
    }

    if (!bus.registerService(QString::fromStdString(bus_name_)))
    {
        string msg = string("Could not acquire bus name: ") + bus_name_ + ": " + bus.lastError().message().toStdString();
        throw ResourceException(msg, int(bus.lastError().type()));
    }
    // TODO: claim bus name
    qDebug() << "Bus unique name:" << bus.baseService();
}

}
}
}
}
