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

#include <unity/storage/provider/internal/DBusPeerCache.h>

#include <utils/DBusEnvironment.h>

#include <gtest/gtest.h>
#include <OnlineAccounts/Account>
#include <OnlineAccounts/Manager>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSignalSpy>

#include <unistd.h>
#include <sys/types.h>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

using namespace std;
using namespace unity::storage::provider;

class DBusPeerCacheTest : public ::testing::Test
{
public:
    QDBusConnection const& connection()
    {
        return dbus_->connection();
    }

protected:
    void SetUp() override
    {
        dbus_.reset(new DBusEnvironment);
        dbus_->start_services();
    }

    void TearDown() override
    {
        dbus_.reset();
    }

    unique_ptr<DBusEnvironment> dbus_;
};

template <typename T>
struct future_result {
    mutex lock;
    T result;
    exception_ptr error;
};

// Wait on a boost::future using the event loop
template <typename T>
T wait_on_future(boost::future<T> &f) {
    struct future_result {
        mutex lock;
        bool complete = false;
        T result;
        exception_ptr error;
    };
    auto r = make_shared<future_result>();

    boost::future<void> f2 = f.then([r](boost::future<T> f) {
            lock_guard<mutex> guard(r->lock);
            r->complete = true;
            try
            {
                r->result = f.get();
            }
            catch (...)
            {
                r->error = current_exception();
            }
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                                      "quit", Qt::QueuedConnection);
        });

    QCoreApplication::instance()->exec();

    lock_guard<mutex> guard(r->lock);
    if (!r->complete)
    {
        throw runtime_error("Future did not complete");
    }
    if (r->error)
    {
        rethrow_exception(r->error);
    }
    return r->result;
}

TEST_F(DBusPeerCacheTest, get_credentials)
{
    // Get the unique name of the Online Accounts manager interface
    QDBusReply<QString> reply = connection().interface()->serviceOwner(
        "com.ubuntu.OnlineAccounts.Manager");
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto peer_name = reply.value();

    internal::DBusPeerCache cache(connection());

    auto f = cache.get(peer_name);
    auto creds = wait_on_future(f);
    EXPECT_TRUE(creds.valid);
    EXPECT_EQ(geteuid(), creds.uid);
    EXPECT_EQ(dbus_->accounts_service_process().processId(), creds.pid);
    // If AppArmor is disabled, this gets filled in with "unconfined" anyway.
    EXPECT_EQ("unconfined", creds.label);

    // repeat
    f = cache.get(peer_name);
    creds = wait_on_future(f);
    EXPECT_TRUE(creds.valid);
    EXPECT_EQ(dbus_->accounts_service_process().processId(), creds.pid);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
