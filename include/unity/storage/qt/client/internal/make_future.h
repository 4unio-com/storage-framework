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

#include <unity/storage/qt/client/Exceptions.h>
#include <unity/storage/qt/client/internal/boost_filesystem.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
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

template<typename T = void>
QFuture<T> make_ready_future()
{
    QFutureInterface<void> qf;
    qf.reportFinished();
    return qf.future();
}

template<typename T = void>
QFuture<T> make_ready_future(QFutureInterface<T> qf)
{
    qf.reportFinished();
    return qf.future();
}

template<typename T>
QFuture<T> make_ready_future(T const& val)
{
    QFutureInterface<T> qf;
    qf.reportResult(val);
    qf.reportFinished();
    return qf.future();
}

template<typename T>
QFuture<T> make_ready_future(QFutureInterface<T> qf, T const& val)
{
    qf.reportResult(val);
    qf.reportFinished();
    return qf.future();
}

template<typename E>
QFuture<void> make_exceptional_future(E const& ex)
{
    QFutureInterface<void> qf;
    qf.reportException(ex);
    qf.reportFinished();
    return qf.future();
}

template<typename T, typename E>
QFuture<T> make_exceptional_future(E const& ex)
{
    QFutureInterface<T> qf;
    qf.reportException(ex);
    qf.reportFinished();
    return qf.future();
}

template<typename T, typename E>
QFuture<T> make_exceptional_future(QFutureInterface<T> qf, E const& ex)
{
    qf.reportException(ex);
    qf.reportFinished();
    return qf.future();
}

template<typename T>
QFuture<T> make_exceptional_future(QString const& msg, boost::filesystem::filesystem_error const& e)
{
    QFutureInterface<T> qf;
    switch (e.code().value())
    {
        case EACCES:
        case EPERM:
        {
            qf.reportException(PermissionException(msg));
            break;
        }
        case EDQUOT:
        case ENOSPC:
        {
            qf.reportException(QuotaException(msg));
            break;
        }
        case ENOENT:
        {
            //qf.reportException(NotExistsException(msg));
            qDebug() << "ENOENT";
            qf.reportException(ResourceException(msg, e.code().value()));
            break;
        }
        default:
        {
            qf.reportException(ResourceException(msg, e.code().value()));
            break;
        }
    }
    qf.reportFinished();
    return qf.future();
}

template<typename T>
QFuture<T> make_exceptional_future(QString const& msg, boost::filesystem::filesystem_error const& e, QString const& key)
{
    QFutureInterface<T> qf;
    switch (e.code().value())
    {
        case ENOENT:
        {
            qf.reportException(NotExistsException(msg, key));
            break;
        }
        default:
        {
            return make_exceptional_future<T>(msg, e);
            break;
        }
    }
    qf.reportFinished();
    return qf.future();
}

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
