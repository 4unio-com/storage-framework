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

#pragma once

#include <unity/storage/visibility.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QFuture>
#pragma GCC diagnostic pop

#include <memory>

class QDBusConnection;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Account;

namespace internal
{

class AccountBase;
class RuntimeBase;

namespace remote_client
{

class AccountImpl;

}  // namespace remote_client

}  // namespace internal

/**
TODO
*/
class UNITY_STORAGE_EXPORT Runtime final
{
public:
    /**
    \brief Destroys the runtime.

    The destructor implicitly calls shutdown().

    \warning Do not invoke methods on any other part of the API once the runtime is destroyed;
    doing so has undefined behavior.
    */
    ~Runtime();

    Runtime(Runtime&&);
    Runtime& operator=(Runtime&&);

    typedef std::shared_ptr<Runtime> SPtr;

    /**
    \brief Initializes the runtime.
    */
    static SPtr create();
    static SPtr create(QDBusConnection const& bus);

    /**
    \brief Shuts down the runtime.

    This method shuts down the runtime. Calling shutdown() more than once is safe and does nothing.

    The destructor implicitly calls shutdown(). This method is provided mainly to permit logging of any
    errors that might arise during shut-down.
    \throws Various exceptions, depending on the error. TODO
    */
    void shutdown();

    QFuture<QVector<std::shared_ptr<Account>>> accounts();

    /// @cond
    /**
    \brief Creates an Account object pointing at (bus_name, object_path)

    This method is intended for use in tests, where you want to talk
    to a provider that has already been set up on the bus.
    */
    std::shared_ptr<Account> make_test_account(QString const& bus_name,
                                               QString const& object_path);
    /// @endcond

private:
    Runtime(internal::RuntimeBase* p) UNITY_STORAGE_HIDDEN;

    std::shared_ptr<internal::RuntimeBase> p_;

    friend class internal::AccountBase;
    friend class internal::remote_client::AccountImpl;
};

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
