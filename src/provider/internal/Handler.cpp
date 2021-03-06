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
    : account_(account), callback_(callback), bus_(bus), message_(message),
      activity_(account->inactivity_timer())
{
}

void Handler::begin()
{
    // If we have already retrieved credentials from OnlineAccounts,
    // and we aren't retrying the request, go to on_authenticated
    // immediately.
    if (account_->has_credentials() && !retry_)
    {
        on_authenticated();
        return;
    }

    // Otherwise, try to authenticate and wait for the result.
    account_->authenticate(true, retry_);
    connect(account_.get(), &AccountData::authenticated,
            this, &Handler::on_authenticated);
}

void Handler::on_authenticated()
{
    disconnect(account_.get(), &AccountData::authenticated,
               this, &Handler::on_authenticated);
    if (!account_->has_credentials())
    {
        string msg = "Handler::begin(): could not retrieve account credentials";
        qDebug() << QString::fromStdString(msg);
        auto ep = make_exception_ptr(UnauthorizedException(msg));
        marshal_exception(ep);
        QMetaObject::invokeMethod(this, "send_reply", Qt::QueuedConnection);
        return;
    }

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
                string msg = "Handler::begin(): could not retrieve D-Bus peer credentials";
                qDebug() << QString::fromStdString(msg);
                auto ep = make_exception_ptr(UnauthorizedException(msg));
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
            catch (UnauthorizedException const& e)
            {
                QMetaObject::invokeMethod(this, "handle_unauthorized",
                                          Qt::QueuedConnection,
                                          Q_ARG(std::exception_ptr, current_exception()));
                return;
            }
            catch (std::exception const& e)
            {
                marshal_exception(current_exception());
            }
            QMetaObject::invokeMethod(this, "send_reply", Qt::QueuedConnection);
        });
}

void Handler::handle_unauthorized(exception_ptr ep)
{
    if (retry_)
    {
        // We've already retried once, so send error out as is.
        marshal_exception(ep);
        send_reply();
    }
    else
    {
        // Otherwise, restart the request with the retry_ flag set.
        retry_ = true;
        begin();
    }
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
