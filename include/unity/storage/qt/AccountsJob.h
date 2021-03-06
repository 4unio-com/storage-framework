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

#include <unity/storage/qt/Account.h>
#include <unity/storage/qt/StorageError.h>

#include <QMetaType>
#include <QObject>

namespace unity
{
namespace storage
{
namespace qt
{
namespace internal
{

class AccountsJobImpl;

}  // namespace internal

class Account;
class StorageError;

class Q_DECL_EXPORT AccountsJob final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isValid READ isValid NOTIFY statusChanged FINAL)
    Q_PROPERTY(unity::storage::qt::AccountsJob::Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(unity::storage::qt::StorageError error READ error NOTIFY statusChanged FINAL)
    Q_PROPERTY(QVariantList accounts READ accountsAsVariantList NOTIFY statusChanged FINAL)

public:
    enum Status { Loading, Finished, Error };
    Q_ENUMS(Status)

    virtual ~AccountsJob();

    bool isValid() const;
    Status status() const;
    StorageError error() const;
    QList<Account> accounts() const;

Q_SIGNALS:
    void statusChanged(unity::storage::qt::AccountsJob::Status status) const;

private:
    AccountsJob(std::unique_ptr<internal::AccountsJobImpl> accounts_job_impl);

    QVariantList accountsAsVariantList() const;

    std::unique_ptr<internal::AccountsJobImpl> const p_;

    friend class internal::AccountsJobImpl;
};

}  // namespace qt
}  // namespace storage
}  // namespace unity

Q_DECLARE_METATYPE(unity::storage::qt::AccountsJob::Status)
