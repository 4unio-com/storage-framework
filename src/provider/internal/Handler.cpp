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

#include <unity/storage/provider/internal/Handler.h>

#include <unity/storage/internal/dbus_error.h>
#include <unity/storage/provider/internal/AccountData.h>
#include <unity/storage/provider/internal/dbusmarshal.h>
#include <unity/storage/provider/internal/DBusPeerCache.h>
#include <unity/storage/provider/internal/MainLoopExecutor.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/Exceptions.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <QDebug>
#pragma GCC diagnostic pop

#include <stdexcept>

using namespace unity::storage::internal;
using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

Handler::Handler(shared_ptr<AccountData> const& account,
                 Callback const& callback,
                 QDBusConnection const& bus, QDBusMessage const& message)
    : account_(account), callback_(callback), bus_(bus), message_(message)
{
}

void Handler::begin()
{
    // Need to put security check in here.
    auto peer_future = account_->dbus_peer().get(message_.service());
    creds_future_ = peer_future.then(
        EXEC_IN_MAIN
        [this](decltype(peer_future) f)
        {
            auto info = f.get();
            if (info.valid)
            {
                context_ = {info.uid, info.pid, std::move(info.label),
                            account_->credentials()};
                QMetaObject::invokeMethod(this, "credentials_received",
                                          Qt::QueuedConnection);
            }
            else
            {
                string msg = "Handler::begin(): could not retrieve credentials";
                qDebug() << QString::fromStdString(msg);
                auto ep = make_exception_ptr(PermissionException(msg));
                marshal_exception(ep);
                QMetaObject::invokeMethod(this, "send_reply",
                                          Qt::QueuedConnection);
            }
        });
}

void Handler::credentials_received()
{
    boost::future<QDBusMessage> msg_future;
    try
    {
        msg_future = callback_(account_, context_, message_);
    }
    catch (std::exception const& e)
    {
        qDebug() << "provider method threw an exception:" << e.what();
        marshal_exception(current_exception());
        QMetaObject::invokeMethod(this, "send_reply", Qt::QueuedConnection);
        return;
    }
    reply_future_ = msg_future.then(
        EXEC_IN_MAIN
        [this](decltype(msg_future) f)
        {
            try
            {
                reply_ = f.get();
            }
            catch (std::exception const& e)
            {
                qDebug() << e.what();
                marshal_exception(current_exception());
            }
            QMetaObject::invokeMethod(this, "send_reply", Qt::QueuedConnection);
        });
}

void Handler::send_reply()
{
    bus_.send(reply_);
    Q_EMIT finished();
}

void Handler::marshal_exception(exception_ptr ep)
{
    try
    {
        rethrow_exception(ep);
    }
    catch (StorageException const& e)
    {
        reply_ = message_.createErrorReply(QString(DBUS_ERROR_PREFIX) + QString::fromStdString(e.type()),
                                           QString::fromStdString(e.error_message()));
        try
        {
            throw;
        }
        catch (NotExistsException const& e)
        {
            reply_ << QVariant(QString::fromStdString(e.key()));
        }
        catch (ExistsException const& e)
        {
            reply_ << QVariant(QString::fromStdString(e.native_identity()));
            reply_ << QVariant(QString::fromStdString(e.name()));
        }
        catch (ResourceException const& e)
        {
            qDebug() << e.what();
            reply_ << QVariant(e.error_code());
        }
        catch (RemoteCommsException const& e)
        {
            qDebug() << e.what();
        }
        catch (UnknownException const& e)
        {
            qDebug() << e.what();
        }
        catch (StorageException const&)
        {
            // Some other sub-type of StorageException without additional data members,
            // and we don't want to log this (not surprising) exception.
        }
    }
    catch (std::exception const& e)
    {
        QString msg = QString("unknown exception thrown by provider: ") + e.what();
        qDebug() << msg;
        reply_ = message_.createErrorReply(QString(DBUS_ERROR_PREFIX) + "UnknownException", msg);
    }
    catch (...)
    {
        QString msg = "unknown exception thrown by provider";
        qDebug() << msg;
        reply_ = message_.createErrorReply(QString(DBUS_ERROR_PREFIX) + "UnknownException", msg);
    }
}

}
}
}
}
