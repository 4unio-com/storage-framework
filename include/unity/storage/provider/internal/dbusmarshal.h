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

#pragma once

#include <unity/storage/provider/ProviderBase.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QDBusArgument>
#include <QVariant>
#pragma GCC diagnostic pop

#include <vector>

namespace unity
{
namespace storage
{
namespace provider
{

struct Item;

QDBusArgument& operator<<(QDBusArgument& argument, Item const& item);
QDBusArgument const& operator>>(QDBusArgument const& argument, Item& item);

QDBusArgument& operator<<(QDBusArgument& argument, std::vector<Item> const& items);
QDBusArgument const& operator>>(QDBusArgument const& argument, std::vector<Item>& items);

}
}
}

Q_DECLARE_METATYPE(unity::storage::provider::Item)
