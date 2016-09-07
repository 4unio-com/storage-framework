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
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <QFuture>
#pragma GCC diagnostic pop
#include <QFutureInterface>

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

template<typename T>
QFuture<T>
__attribute__ ((warn_unused_result))
make_ready_future(T const& val)
{
    QFutureInterface<T> qf;
    qf.reportResult(val);
    qf.reportFinished();
    return qf.future();
}

template<typename T = void>
QFuture<T>
__attribute__ ((warn_unused_result))
make_ready_future()
{
    QFutureInterface<void> qf;
    return make_ready_future(qf);
}

template<typename E>
QFuture<void>
__attribute__
((warn_unused_result)) make_exceptional_future(E const& ex)
{
    QFutureInterface<void> qf;
    qf.reportException(ex);
    qf.reportFinished();
    return qf.future();
}

template<typename T, typename E>
QFuture<T>
__attribute__ ((warn_unused_result))
make_exceptional_future(E const& ex)
{
    QFutureInterface<T> qf;
    qf.reportException(ex);
    qf.reportFinished();
    return qf.future();
}

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
