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

#include <unity/storage/provider/Server.h>
#include <unity/storage/internal/TraceMessageHandler.h>
#include <unity/storage/provider/internal/DBusPeerCache.h>
#include <unity/storage/provider/internal/ProviderInterface.h>

#include <OnlineAccounts/Manager>
#include <OnlineAccounts/Account>
#include <QObject>
#include <QCoreApplication>
#include <QDBusConnection>

#include <map>
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

class ServerImpl : public QObject {
    Q_OBJECT
public:
    ServerImpl(ServerBase* server, std::string const& bus_name, std::string const& account_service_id);
    ~ServerImpl();

    void init(int& argc, char **argv);
    void run();

private Q_SLOTS:
    void account_manager_ready();

private:
    ServerBase* const server_;
    std::string const bus_name_;
    std::string const service_id_;
    unity::storage::internal::TraceMessageHandler trace_message_handler_;

    std::unique_ptr<QCoreApplication> app_;
    std::unique_ptr<OnlineAccounts::Manager> manager_;
    std::shared_ptr<DBusPeerCache> dbus_peer_;
    std::map<OnlineAccounts::AccountId,std::unique_ptr<ProviderInterface>> interfaces_;

    Q_DISABLE_COPY(ServerImpl)
};

}
}
}
}
