/*
 * Copyright (C) 2015 Canonical Ltd
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
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <QDebug>
#pragma GCC diagnostic pop

namespace unity
{
namespace storage
{
namespace internal
{

class TraceMessageHandler final
{
public:
    TraceMessageHandler();
    TraceMessageHandler(std::string const& prog_name);
    TraceMessageHandler(QString const& prog_name);
    TraceMessageHandler(char const* prog_name);
    ~TraceMessageHandler();

private:
    QtMessageHandler old_message_handler_;
};

}  // namespace internal
}  // namespace storage
}  // namespace unity
