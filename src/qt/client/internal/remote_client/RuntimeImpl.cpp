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

#include <unity/storage/qt/client/internal/remote_client/RuntimeImpl.h>

#include <unity/storage/internal/dbusmarshal.h>
#include <unity/storage/qt/client/Account.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/remote_client/AccountImpl.h>
#include <unity/storage/internal/dbusmarshal.h>

#include <QDBusMetaType>

// TODO: Hack until we can use the registry instead
#include <OnlineAccounts/Account>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <glib.h>
#pragma GCC diagnostic pop

#include <cassert>
#include <cstdlib>

#include <unistd.h>

using namespace std;

namespace
{

static const map<QString, QString> BUS_NAMES =
{
    { "google-drive-scope", "com.canonical.StorageFramework.Provider.ProviderTest" },
    { "com.canonical.scopes.mcloud_mcloud_mcloud", "com.canonical.StorageFramework.Provider.McloudProvider" }
};

}  // namespace

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
namespace remote_client
{

RuntimeImpl::RuntimeImpl(QDBusConnection const& bus)
    : conn_(bus)
{
    if (!conn_.isConnected())
    {
        throw LocalCommsException("Runtime: cannot connect to session bus");  // LCOV_EXCL_LINE
    }
    qDBusRegisterMetaType<unity::storage::internal::ItemMetadata>();
    qDBusRegisterMetaType<QList<unity::storage::internal::ItemMetadata>>();
}

RuntimeImpl::~RuntimeImpl()
{
    try
    {
        shutdown();
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        qCritical() << "shutdown error" << e.what();
    }
    // LCOV_EXCL_STOP
}

void RuntimeImpl::shutdown()
{
    if (destroyed_)
    {
        return;
    }
    destroyed_ = true;
    conn_.disconnectFromBus(conn_.name());
}

QFuture<QVector<Account::SPtr>> RuntimeImpl::accounts()
{
    if (destroyed_)
    {
        qf_.reportException(RuntimeDestroyedException("Runtime::accounts()"));
        qf_.reportFinished();
        return qf_.future();
    }

    if (!manager_)
    {
        manager_.reset(new OnlineAccounts::Manager("", conn_));
        connect(manager_.get(), &OnlineAccounts::Manager::ready, this, &RuntimeImpl::manager_ready);
        connect(&timer_, &QTimer::timeout, this, &RuntimeImpl::timeout);
        timer_.setSingleShot(true);
        timer_.start(5000);
    }

    qf_.reportStarted();
    return qf_.future();
}

QDBusConnection& RuntimeImpl::connection()
{
    return conn_;
}

void RuntimeImpl::manager_ready()
{
    if (destroyed_)
    {
        // LCOV_EXCL_START
        qf_.reportException(RuntimeDestroyedException("Runtime::accounts()"));
        qf_.reportFinished();
        return;
        // LCOV_EXCL_STOP
    }

    timer_.stop();

    try
    {
        QVector<Account::SPtr> accounts;
        for (auto const map_entry : BUS_NAMES)
        {
            auto service_id = map_entry.first;
            for (auto const& a : manager_->availableAccounts(service_id))
            {
                auto object_path = QStringLiteral("/provider/%1").arg(a->id());
                try
                {
                    auto bus_name = map_entry.second;
                    accounts.append(make_account(bus_name, object_path,
                                                 "", a->serviceId(), a->displayName()));

                }
                catch (LocalCommsException const& e)
                {
                    qDebug() << "RuntimeImpl: ignoring non-existent provider" << a->serviceId();
                }
            }
        }
        accounts_ = accounts;
        qf_.reportResult(accounts);
    }
    // LCOV_EXCL_START
    catch (StorageException const& e)
    {
        qf_.reportException(e);
    }
    // LCOV_EXCL_STOP
    qf_.reportFinished();
}

// LCOV_EXCL_START
void RuntimeImpl::timeout()
{
    qf_.reportException(ResourceException("Runtime::accounts(): timeout retrieving Online accounts", 0));
    qf_.reportFinished();
}
// LCOV_EXCL_STOP

shared_ptr<Account> RuntimeImpl::make_test_account(QString const& bus_name,
                                                   QString const& object_path)
{
    return make_account(bus_name, object_path, "", "", "");
}

shared_ptr<Account> RuntimeImpl::make_account(QString const& bus_name,
                                              QString const& object_path,
                                              QString const& owner,
                                              QString const& owner_id,
                                              QString const& description)
{
    auto impl = new AccountImpl(public_instance_, bus_name, object_path,
                                owner, owner_id, description);
    Account::SPtr acc(new Account(impl));
    impl->set_public_instance(acc);
    return acc;
}


}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
