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

#include <unity/storage/internal/dbusmarshal.h>

#include <QDebug>

using namespace unity::storage::internal;
using namespace std;

namespace unity
{
namespace storage
{
namespace internal
{

QDBusArgument& operator<<(QDBusArgument& argument, storage::internal::ItemMetadata const& metadata)
{
    argument.beginStructure();
    argument << metadata.item_id;
    argument << metadata.parent_ids;
    argument << metadata.name;
    argument << metadata.etag;
    argument << static_cast<int32_t>(metadata.type);
    argument.beginMap(QVariant::String, qMetaTypeId<QDBusVariant>());
    decltype(ItemMetadata::metadata)::const_iterator i = metadata.metadata.constBegin();
    while (i != metadata.metadata.constEnd())
    {
        argument.beginMapEntry();
        argument << i.key() << QDBusVariant(i.value());
        argument.endMapEntry();
        ++i;
    }
    argument.endMap();
    argument.endStructure();
    return argument;
}

QDBusArgument const& operator>>(QDBusArgument const& argument, storage::internal::ItemMetadata& metadata)
{
    argument.beginStructure();
    argument >> metadata.item_id;
    argument >> metadata.parent_ids;
    argument >> metadata.name;
    argument >> metadata.etag;
    int32_t enum_val;
    argument >> enum_val;
    if (enum_val < 0 || enum_val >= int(ItemType::LAST_ENTRY__))
    {
        qCritical() << "unmarshaling error: impossible ItemType value: " + QString::number(enum_val);
        return argument;  // Forces error
    }
    metadata.type = static_cast<ItemType>(enum_val);
    metadata.metadata.clear();
    argument.beginMap();
    while (!argument.atEnd())
    {
        QString key;
        QVariant value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        metadata.metadata.insert(key, value);
    }
    argument.endMap();
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, QList<storage::internal::ItemMetadata> const& md_list)
{
    argument.beginArray(qMetaTypeId<storage::internal::ItemMetadata>());
    for (auto const& md : md_list)
    {
        argument << md;
    }
    argument.endArray();
    return argument;
}

QDBusArgument const& operator>>(QDBusArgument const& argument, QList<storage::internal::ItemMetadata>& md_list)
{
    md_list.clear();
    argument.beginArray();
    while (!argument.atEnd())
    {
        ItemMetadata imd;
        argument >> imd;
        md_list.append(imd);
    }
    argument.endArray();
    return argument;
}

}  // namespace internal
}  // namespace storage
}  // namespace unity
