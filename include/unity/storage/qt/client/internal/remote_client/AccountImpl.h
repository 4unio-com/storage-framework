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

#include <unity/storage/qt/client/internal/AccountBase.h>

class ProviderInterface;

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

class AccountImpl : public QObject, public virtual AccountBase
{
public:
    AccountImpl(std::weak_ptr<Runtime> const& runtime,
                QString const& bus_name,
                QString const& object_path,
                QString const& owner,
                QString const& owner_id,
                QString const& description);

    virtual QString owner() const override;
    virtual QString owner_id() const override;
    virtual QString description() const override;
    virtual QFuture<QVector<std::shared_ptr<Root>>> roots() override;

    std::shared_ptr<ProviderInterface> provider() const noexcept;

private:
    QString owner_;
    QString owner_id_;
    QString description_;
    QVector<std::shared_ptr<Root>> roots_;
    std::shared_ptr<ProviderInterface> provider_;
};

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
