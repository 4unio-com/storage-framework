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

#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/internal/AccountData.h>
#include <unity/storage/provider/internal/DBusPeerCache.h>

#include <utils/DBusEnvironment.h>

#include <gtest/gtest.h>
#include <OnlineAccounts/Account>
#include <OnlineAccounts/Manager>
#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>

using namespace std;
using namespace unity::storage::provider;

class AccountDataTest : public ::testing::Test
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

private:
    unique_ptr<DBusEnvironment> dbus_;
};

TEST_F(AccountDataTest, oauth1_credentials)
{
    OnlineAccounts::Manager manager("", connection());
    manager.waitForReady();
    ASSERT_TRUE(manager.isReady());

    auto accounts = manager.availableAccounts("oauth1-service");
    ASSERT_EQ(1, accounts.size());

    internal::AccountData account(unique_ptr<ProviderBase>(),
                                  shared_ptr<internal::DBusPeerCache>(),
                                  connection(),
                                  accounts[0]);

    QSignalSpy spy(&account, &internal::AccountData::authenticated);
    account.authenticate(true);
    ASSERT_TRUE(spy.wait());

    ASSERT_TRUE(account.has_credentials());
    auto creds = boost::get<OAuth1Credentials>(account.credentials());

    EXPECT_EQ("consumer_key", creds.consumer_key);
    EXPECT_EQ("consumer_secret", creds.consumer_secret);
    EXPECT_EQ("token", creds.token);
    EXPECT_EQ("token_secret", creds.token_secret);
}

TEST_F(AccountDataTest, oauth2_credentials)
{
    OnlineAccounts::Manager manager("", connection());
    manager.waitForReady();
    ASSERT_TRUE(manager.isReady());

    auto accounts = manager.availableAccounts("oauth2-service");
    ASSERT_EQ(1, accounts.size());

    internal::AccountData account(unique_ptr<ProviderBase>(),
                                  shared_ptr<internal::DBusPeerCache>(),
                                  connection(),
                                  accounts[0]);

    QSignalSpy spy(&account, &internal::AccountData::authenticated);
    account.authenticate(true);
    ASSERT_TRUE(spy.wait());

    ASSERT_TRUE(account.has_credentials());
    auto creds = boost::get<OAuth2Credentials>(account.credentials());

    EXPECT_EQ("access_token", creds.access_token);
}

TEST_F(AccountDataTest, password_credentials)
{
    OnlineAccounts::Manager manager("", connection());
    manager.waitForReady();
    ASSERT_TRUE(manager.isReady());

    auto accounts = manager.availableAccounts("password-service");
    ASSERT_EQ(1, accounts.size());

    internal::AccountData account(unique_ptr<ProviderBase>(),
                                  shared_ptr<internal::DBusPeerCache>(),
                                  connection(),
                                  accounts[0]);

    QSignalSpy spy(&account, &internal::AccountData::authenticated);
    account.authenticate(true);
    ASSERT_TRUE(spy.wait());

    ASSERT_TRUE(account.has_credentials());
    auto creds = boost::get<PasswordCredentials>(account.credentials());

    EXPECT_EQ("user", creds.username);
    EXPECT_EQ("pass", creds.password);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
