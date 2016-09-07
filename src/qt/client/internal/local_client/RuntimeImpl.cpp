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

#include <unity/storage/qt/client/internal/local_client/RuntimeImpl.h>

#include <unity/storage/qt/client/Account.h>
#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/local_client/AccountImpl.h>

#include <QAbstractSocket>
#include <QFutureInterface>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <glib.h>
#pragma GCC diagnostic pop

#include <cassert>
#include <cstdlib>

#include <unistd.h>

using namespace std;

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
namespace local_client
{

RuntimeImpl::RuntimeImpl()
{
    qRegisterMetaType<QAbstractSocket::SocketState>();
}

RuntimeImpl::~RuntimeImpl()
{
    try
    {
        shutdown();
    }
    catch (std::exception const&)
    {
    }
}

void RuntimeImpl::shutdown()
{
    if (destroyed_)
    {
        return;
    }
    destroyed_ = true;
}

QFuture<QVector<Account::SPtr>> RuntimeImpl::accounts()
{
    if (destroyed_)
    {
        return internal::make_exceptional_future<QVector<Account::SPtr>>(RuntimeDestroyedException("Runtime::accounts()"));
    }

    char const* user = g_get_user_name();
    assert(*user != '\0');
    QString owner = user;

    QString owner_id;
    owner_id.setNum(getuid());

    QString description = "Account for " + owner + " (" + owner_id + ")";

    QFutureInterface<QVector<Account::SPtr>> qf;

    if (!accounts_.isEmpty())
    {
        return make_ready_future(accounts_);
    }

    // Create accounts_ on first access.
    auto impl = new AccountImpl(public_instance_, owner, owner_id, description);
    Account::SPtr acc(new Account(impl));
    impl->set_public_instance(acc);
    accounts_.append(acc);
    return make_ready_future(accounts_);
}

shared_ptr<Account> RuntimeImpl::make_test_account(QString const& bus_name,
                                                   QString const& object_path)
{
    Q_UNUSED(bus_name);
    Q_UNUSED(object_path);
    throw LocalCommsException("Can not create test account with local client");
}


}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
