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

#pragma once

#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/internal/DBusPeerCache.h>

#include <boost/thread/future.hpp>

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>

#include <functional>
#include <memory>

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

class AccountData;
class PendingJobs;

class Handler : public QObject
{
    Q_OBJECT
public:
    typedef std::function<boost::future<QDBusMessage>(std::shared_ptr<AccountData> const&, Context const&, QDBusMessage const&)> Callback;

    Handler(std::shared_ptr<AccountData> const& account,
            Callback const& callback,
            QDBusConnection const& bus, QDBusMessage const& message);

public Q_SLOTS:
    void begin();

private Q_SLOTS:
    void credentials_received();
    void send_reply();

Q_SIGNALS:
    void finished();

private:
    void marshal_exception(std::exception_ptr ep);

    std::shared_ptr<AccountData> const account_;
    Callback const callback_;
    QDBusConnection const bus_;
    QDBusMessage const message_;

    boost::future<void> creds_future_;
    boost::future<void> reply_future_;
    Context context_;
    QDBusMessage reply_;

    Q_DISABLE_COPY(Handler)
};

}
}
}
}
