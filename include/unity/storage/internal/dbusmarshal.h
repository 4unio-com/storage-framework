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

#include <unity/storage/internal/ItemMetadata.h>

#include <QDBusArgument>
#include <QMetaType>

namespace unity
{
namespace storage
{
namespace internal
{

QDBusArgument& operator<<(QDBusArgument& argument, ItemMetadata const& metadata);
QDBusArgument const& operator>>(QDBusArgument const& argument, ItemMetadata& metadata);

QDBusArgument& operator<<(QDBusArgument& argument, QList<ItemMetadata> const& md_list);
QDBusArgument const& operator>>(QDBusArgument const& argument, QList<ItemMetadata>& md_list);

}  // namespace internal
}  // storage
}  // unity

Q_DECLARE_METATYPE(unity::storage::internal::ItemMetadata)
Q_DECLARE_METATYPE(QList<unity::storage::internal::ItemMetadata>)
