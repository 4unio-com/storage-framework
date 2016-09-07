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

#include <unity/storage/qt/client/Account.h>

#include <unity/storage/qt/client/internal/AccountBase.h>

#include <cassert>

using namespace std;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

Account::Account(internal::AccountBase* p)
    : p_(p)
{
    assert(p != nullptr);
}

Account::~Account() = default;

shared_ptr<Runtime> Account::runtime() const
{
    return p_->runtime();
}

QString Account::owner() const
{
    return p_->owner();
}

QString Account::owner_id() const
{
    return p_->owner_id();
}

QString Account::description() const
{
    return p_->description();
}

QFuture<QVector<std::shared_ptr<Root>>> Account::roots() const
{
    return p_->roots();
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
