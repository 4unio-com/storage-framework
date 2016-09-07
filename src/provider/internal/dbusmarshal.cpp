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

#include <unity/storage/provider/internal/dbusmarshal.h>
#include <unity/storage/provider/ProviderBase.h>

#include <cstdint>
#include <stdexcept>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{

namespace
{

QDBusVariant to_qdbus_variant(MetadataValue const& v)
{
    switch (v.which())
    {
        case 0:
            return QDBusVariant(QString::fromStdString(boost::get<string>(v)));
        case 1:
            return QDBusVariant(qlonglong(boost::get<int64_t>(v)));
        default:
            abort();  // Impossible.  // LCOV_EXCL_LINE
    }
}

}  // namespace

QDBusArgument& operator<<(QDBusArgument& argument, Item const& item)
{
    argument.beginStructure();
    argument << QString::fromStdString(item.item_id);
    {
        argument.beginArray(qMetaTypeId<QString>());
        for (auto const& id : item.parent_ids)
        {
            argument << QString::fromStdString(id);
        }
        argument.endArray();
    }
    argument << QString::fromStdString(item.name);
    argument << QString::fromStdString(item.etag);
    argument << static_cast<int32_t>(item.type);
    {
        argument.beginMap(QVariant::String, qMetaTypeId<QDBusVariant>());
        for (auto const& pair : item.metadata)
        {
            argument.beginMapEntry();
            argument << QString::fromStdString(pair.first) << to_qdbus_variant(pair.second);
            argument.endMapEntry();
        }
        argument.endMap();
    }
    argument.endStructure();
    return argument;
}

QDBusArgument const& operator>>(QDBusArgument const&, Item&)
{
    // We don't expect to ever have to unmarshal anything, only marshal it.
    qFatal("unexpected call to operator>>(QDBusArgument const&, Item&)");  // LCOV_EXCL_LINE
}

QDBusArgument& operator<<(QDBusArgument& argument, ItemList const& items)
{
    argument.beginArray(qMetaTypeId<Item>());
    for (auto const& item : items)
    {
        argument << item;
    }
    argument.endArray();
    return argument;
}

QDBusArgument const& operator>>(QDBusArgument const&, ItemList&)
{
    // We don't expect to ever have to unmarshal anything, only marshal it.
    qFatal("unexpected call to operator>>(QDBusArgument const&, ItemList&)");  // LCOV_EXCL_LINE
}

}
}
}
