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

#include <unordered_map>

namespace unity
{
namespace storage
{
namespace provider
{

static char constexpr SIZE_IN_BYTES[] = "size_in_bytes";            // int64_t, >= 0
static char constexpr CREATION_TIME[] = "creation_time";            // String, ISO 8601 format
static char constexpr LAST_MODIFIED_TIME[] = "last_modified_time";  // String, ISO 8601 format

enum class MetadataType { int64, iso_8601_date_time };

static std::unordered_map<std::string, MetadataType> known_metadata =
{
    { SIZE_IN_BYTES, MetadataType::int64 },
    { CREATION_TIME, MetadataType::iso_8601_date_time },
    { LAST_MODIFIED_TIME, MetadataType::iso_8601_date_time }
};

}  // namespace provider
}  // namespace storage
}  // namespace unity
