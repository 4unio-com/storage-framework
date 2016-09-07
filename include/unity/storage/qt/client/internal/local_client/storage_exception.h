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
#include <unity/storage/qt/client/internal/local_client/boost_filesystem.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <QFuture>
#pragma GCC diagnostic pop
#include <QFutureInterface>

class QString;

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
namespace local_client
{

void throw_storage_exception(QString const& method,
                             std::exception_ptr ep) __attribute__ ((noreturn));

void throw_storage_exception(QString const& method,
                             std::exception_ptr ep,
                             QString const& key) __attribute__ ((noreturn));

template<typename T>
QFuture<T> make_exceptional_future(QString const& method, std::exception_ptr ep)
{
    try
    {
        throw_storage_exception(method, ep);
    }
    catch (StorageException const& e)
    {
        QFutureInterface<T> qf;
        qf.reportException(e);
        qf.reportFinished();
        return qf.future();
    }
    abort();  // Impossible.  // LCOV_EXCL_LINE
}

template<typename T>
QFuture<T> make_exceptional_future(QString const& method,
                                   std::exception_ptr ep,
                                   QString const& key)
{
    try
    {
        throw_storage_exception(method, ep, key);
    }
    catch (StorageException const& e)
    {
        QFutureInterface<T> qf;
        qf.reportException(e);
        qf.reportFinished();
        return qf.future();
    }
    abort();  // Impossible.  // LCOV_EXCL_LINE
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
