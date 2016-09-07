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

class Account;
class Runtime;

namespace internal
{

class AccountBase;

class RuntimeBase : public QObject
{
public:
    RuntimeBase() = default;
    virtual ~RuntimeBase() = default;
    RuntimeBase(RuntimeBase const&) = delete;
    RuntimeBase& operator=(RuntimeBase const&) = delete;

    virtual void shutdown() = 0;
    virtual QFuture<QVector<std::shared_ptr<Account>>> accounts() = 0;
    virtual std::shared_ptr<Account> make_test_account(QString const& bus_name,
                                                       QString const& object_path) = 0;

    void set_public_instance(std::weak_ptr<Runtime> p);

protected:
    bool destroyed_ = false;
    QVector<std::shared_ptr<Account>> accounts_;
    std::weak_ptr<Runtime> public_instance_;      // Immutable once set

    friend class unity::storage::qt::client::internal::AccountBase;
};

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
