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
#include <unity/storage/qt/client/internal/make_future.h>
#include <unity/storage/qt/client/internal/remote_client/dbusmarshal.h>
#include <unity/storage/qt/client/internal/remote_client/HandlerBase.h>

#include <QDBusPendingReply>

#include <cassert>

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
namespace remote_client
{

template<typename T>
class Handler : public HandlerBase
{
public:
    template<typename ... DBusArgs>
    Handler(QObject* parent,
            QDBusPendingReply<DBusArgs...>& reply,
            std::function<void(decltype(reply)&, QFutureInterface<T>&)> closure);

    QFuture<T> future();

private:
    QFutureInterface<T> qf_;
};

// TODO: HACK: The reply argument really should be passed by const reference, which also
//             would make the explicit conversion of the call to QDBusPendingReply<QDBusArgs...>
//             unnecessary. But this doesn't work with gcc 4.9: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60420
//             Once we get rid of Vivid, this should be changed back to
//
//             Handler<T>::Handler(QObject* parent,
//                                 QDBusPendingReply<DBusArgs...> const& reply,
//                                 std::function<void(decltype(reply) const&, QFutureInterface<T>&)> closure)

template<typename T>
template<typename ... DBusArgs>
Handler<T>::Handler(QObject* parent,
                    QDBusPendingReply<DBusArgs...>& reply,
                    std::function<void(decltype(reply)&, QFutureInterface<T>&)> closure)
    : HandlerBase(parent,
                  reply,
                  [this, closure](QDBusPendingCallWatcher& call)
                      {
                          if (call.isError())
                          {
                              try
                              {
                                  auto ep = unmarshal_exception(call);
                                  std::rethrow_exception(ep);
                              }
                              // We catch some exceptions that are "surprising" so we can log those.
                              catch (LocalCommsException const& e)
                              {
                                  qCritical() << "provider exception:" << e.what();
                                  qf_.reportException(e);
                                  qf_.reportFinished();
                              }
                              catch (RemoteCommsException const& e)
                              {
                                  qCritical() << "provider exception:" << e.what();
                                  qf_.reportException(e);
                                  qf_.reportFinished();
                              }
                              catch (ResourceException const& e)
                              {
                                  qCritical() << "provider exception:" << e.what();
                                  qf_.reportException(e);
                                  qf_.reportFinished();
                              }
                              catch (StorageException const& e)
                              {
                                  qf_.reportException(e);
                                  qf_.reportFinished();
                              }
                              // LCOV_EXCL_START
                              catch (...)
                              {
                                  abort();  // Impossible.
                              }
                              // LCOV_EXCL_STOP
                              return;
                          }
                          // TODO: See HACK above. Should just be closure(call, qf_);
                          QDBusPendingReply<DBusArgs...> r = call;
                          closure(r, qf_);
                      })
{
    qf_.reportStarted();
}

template<typename T>
QFuture<T> Handler<T>::future()
{
    return qf_.future();
}

}  // namespace remote_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
