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
#include <QVector>

#include <memory>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Runtime;
class Root;

namespace internal
{

class AccountBase;

namespace local_client
{

class RuntimeImpl;

}  // namespace local_client

namespace remote_client
{

class ItemImpl;
class RuntimeImpl;

}  // namespace remote_client
}  // namespace internal

/**
\brief Class that represents an account.
*/
class UNITY_STORAGE_EXPORT Account final
{
public:
    /// @cond
    ~Account();
    /// @endcond

    Account(Account&&);
    Account& operator=(Account&&);

    typedef std::shared_ptr<Account> SPtr;

    std::shared_ptr<Runtime> runtime() const;

    QString owner() const;
    QString owner_id() const;
    QString description() const;

    // TODO: Will almost certainly need more here. Other details?

    /**
    \brief Returns the root directories for the account.

    An account can have more than one root directory (for providers that support the concept of multiple drives).
    */
    QFuture<QVector<std::shared_ptr<Root>>> roots() const;

private:
    Account(internal::AccountBase*) UNITY_STORAGE_HIDDEN;

    std::shared_ptr<internal::AccountBase> p_;

    friend class internal::local_client::RuntimeImpl;
    friend class internal::remote_client::ItemImpl;
    friend class internal::remote_client::RuntimeImpl;
};

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
