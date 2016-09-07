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

#include <QDBusConnection>
#include <QProcess>
#include <QSharedPointer>
#include <memory>

namespace QtDBusTest
{
class DBusTestRunner;
class QProcessDBusService;
}

class DBusEnvironment final {
public:
    DBusEnvironment();
    ~DBusEnvironment();

    QDBusConnection const& connection() const;
    QString const& busAddress() const;

    void add_demo_provider(char const* service_id);
    void start_services();

    QProcess& accounts_service_process();

private:
    std::unique_ptr<QtDBusTest::DBusTestRunner> runner_;
    QSharedPointer<QtDBusTest::QProcessDBusService> accounts_service_;
    QSharedPointer<QtDBusTest::QProcessDBusService> demo_provider_;
};
