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

#include <unity/storage/internal/dbus_error.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/testing/TestServer.h>

#include "TestProvider.h"

#include <utils/ProviderFixture.h>

#include <gtest/gtest.h>
#include <OnlineAccounts/Account>
#include <OnlineAccounts/Manager>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTimer>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

using namespace std;
using unity::storage::ItemType;
using unity::storage::provider::ProviderBase;
using unity::storage::provider::testing::TestServer;

namespace {

const auto SECOND_CONNECTION_NAME = QStringLiteral("second-bus-connection");

const QString PROVIDER_ERROR = unity::storage::internal::DBUS_ERROR_PREFIX;

const string file_contents =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
    "enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor "
    "in reprehenderit in voluptate velit esse cillum dolore eu fugiat "
    "nulla pariatur. Excepteur sint occaecat cupidatat non proident, "
    "sunt in culpa qui officia deserunt mollit anim id est laborum.";
}

class ProviderInterfaceTest : public ProviderFixture
{
protected:
    void SetUp() override
    {
        client_.reset(new ProviderClient(bus_name(), object_path(), connection()));
    }

    void TearDown() override
    {
        client_.reset();
    }

    std::unique_ptr<ProviderClient> client_;
};

TEST_F(ProviderInterfaceTest, roots)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Roots();
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    EXPECT_EQ(1, reply.value().size());
    auto root = reply.value()[0];
    EXPECT_EQ("root_id", root.item_id);
    EXPECT_EQ(QVector<QString>(), root.parent_ids);
    EXPECT_EQ("Root", root.name);
    EXPECT_EQ("etag", root.etag);
    EXPECT_EQ(ItemType::root, root.type);
}

TEST_F(ProviderInterfaceTest, list)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->List("root_id", "");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto items = reply.argumentAt<0>();
    QString page_token = reply.argumentAt<1>();
    ASSERT_EQ(2, items.size());
    EXPECT_EQ("child1_id", items[0].item_id);
    EXPECT_EQ("child2_id", items[1].item_id);
    EXPECT_EQ("page_token", page_token);

    reply = client_->List("root_id", page_token);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    items = reply.argumentAt<0>();
    page_token = reply.argumentAt<1>();
    ASSERT_EQ(2, items.size());
    EXPECT_EQ("child3_id", items[0].item_id);
    EXPECT_EQ("child4_id", items[1].item_id);
    EXPECT_EQ("", page_token);

    // Try a bad page token
    reply = client_->List("root_id", "bad_page_token");
    wait_for(reply);
    EXPECT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name()) << reply.error().name().toStdString();
    EXPECT_EQ("Unknown page token", reply.error().message()) << reply.error().message().toStdString();

    reply = client_->List("no_such_folder_id", "");
    wait_for(reply);
    EXPECT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "NotExistsException", reply.error().name());
    EXPECT_EQ("Unknown folder", reply.error().message());
}

TEST_F(ProviderInterfaceTest, lookup)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Lookup("root_id", "Filename");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto items = reply.value();
    ASSERT_EQ(1, items.size());
    auto item = items[0];
    EXPECT_EQ("child_id", item.item_id);
    EXPECT_EQ(QVector<QString>{ "root_id"}, item.parent_ids);
    EXPECT_EQ("Filename", item.name);
    EXPECT_EQ(ItemType::file, item.type);
}

TEST_F(ProviderInterfaceTest, metadata)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Metadata("root_id");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("root_id", item.item_id);
    EXPECT_EQ(QVector<QString>(), item.parent_ids);
    EXPECT_EQ("Root", item.name);
    EXPECT_EQ(ItemType::root, item.type);
}

TEST_F(ProviderInterfaceTest, create_folder)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->CreateFolder("root_id", "New Folder");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("new_folder_id", item.item_id);
    EXPECT_EQ(QVector<QString>{ "root_id" }, item.parent_ids);
    EXPECT_EQ("New Folder", item.name);
    EXPECT_EQ(ItemType::folder, item.type);
}

TEST_F(ProviderInterfaceTest, create_file)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->CreateFile("parent_id", "file name", file_contents.size(), "text/plain", false);
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Write);
    size_t total_written = 0;
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [app, &notifier, &total_written](int fd) {
            ssize_t n_written = write(fd, file_contents.data() + total_written, file_contents.size() - total_written);
            if (n_written < 0)
            {
                // Error writing
                notifier.setEnabled(false);
                app->quit();
            }
            total_written += n_written;
            if (total_written == file_contents.size())
            {
                notifier.setEnabled(false);
                app->quit();
            }
        });
    notifier.setEnabled(true);
    app->exec();
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));

    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("new_file_id", item.item_id);
    EXPECT_EQ(QVector<QString>{ "parent_id" }, item.parent_ids);
    EXPECT_EQ("file name", item.name);
}

TEST_F(ProviderInterfaceTest, update)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("item_id", file_contents.size(), "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Write);
    size_t total_written = 0;
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [app, &notifier, &total_written](int fd) {
            ssize_t n_written = write(fd, file_contents.data() + total_written, file_contents.size() - total_written);
            if (n_written < 0)
            {
                // Error writing
                notifier.setEnabled(false);
                app->quit();
            }
            total_written += n_written;
            if (total_written == file_contents.size())
            {
                notifier.setEnabled(false);
                app->quit();
            }
        });
    notifier.setEnabled(true);
    app->exec();
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));

    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("item_id", item.item_id);
}

TEST_F(ProviderInterfaceTest, upload_short_write)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));
    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("wrong number of bytes", reply.error().message());
}

TEST_F(ProviderInterfaceTest, upload_long_write)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("item_id", file_contents.size() - 5, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Write);
    size_t total_written = 0;
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [app, &notifier, &total_written](int fd) {
            ssize_t n_written = write(fd, file_contents.data() + total_written, file_contents.size() - total_written);
            if (n_written < 0)
            {
                // Error writing
                notifier.setEnabled(false);
                app->quit();
            }
            total_written += n_written;
            if (total_written == file_contents.size())
            {
                notifier.setEnabled(false);
                app->quit();
            }
        });
    notifier.setEnabled(true);
    app->exec();
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));

    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("too many bytes written", reply.error().message().toStdString());
}

TEST_F(ProviderInterfaceTest, upload_not_closed)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("Socket not closed", reply.error().message());
}

TEST_F(ProviderInterfaceTest, cancel_upload)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    auto reply = client_->CancelUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
}

TEST_F(ProviderInterfaceTest, cancel_upload_wrong_connection)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto upload_reply = client_->Update("item_id", 100, "old_etag");
    wait_for(upload_reply);
    ASSERT_TRUE(upload_reply.isValid()) << upload_reply.error().message().toStdString();
    auto upload_id = upload_reply.argumentAt<0>();

    // Try to finish download using a second connection
    QDBusConnection connection2 = QDBusConnection::connectToBus(dbus_->busAddress(), SECOND_CONNECTION_NAME);
    QDBusConnection::disconnectFromBus(SECOND_CONNECTION_NAME);
    ProviderClient client2(bus_name(), object_path(), connection2);
    auto reply = client2.CancelUpload(upload_id);
    wait_for(reply);
    ASSERT_FALSE(reply.isValid());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_TRUE(reply.error().message().startsWith("No such upload: ")) << reply.error().message().toStdString();
}

TEST_F(ProviderInterfaceTest, cancel_upload_on_disconnect)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QDBusServiceWatcher service_watcher;
    service_watcher.setConnection(*service_connection_);
    service_watcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    QSignalSpy service_spy(
        &service_watcher, &QDBusServiceWatcher::serviceUnregistered);

    QDBusUnixFileDescriptor socket;
    {
        QDBusConnection connection2 = QDBusConnection::connectToBus(dbus_->busAddress(), SECOND_CONNECTION_NAME);
        QDBusConnection::disconnectFromBus(SECOND_CONNECTION_NAME);
        service_watcher.addWatchedService(connection2.baseService());
        ProviderClient client2(bus_name(), object_path(), connection2);
        auto reply = client2.Update("item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        // Store socket so it will remain open past the closing of the
        // D-Bus connection.
        socket = reply.argumentAt<1>();
    }

    // Wait until we're sure the fact that connection2 closed has
    // reached the service's connection, and then a little more to
    // ensure it is triggered.
    if (service_spy.count() == 0)
    {
        ASSERT_TRUE(service_spy.wait());
    }
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(100);
    timer.start();
    QSignalSpy timer_spy(&timer, &QTimer::timeout);
    ASSERT_TRUE(timer_spy.wait());
}

TEST_F(ProviderInterfaceTest, finish_upload_unknown)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->FinishUpload("no-such-upload");
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("No such upload: no-such-upload", reply.error().message());
}

TEST_F(ProviderInterfaceTest, finish_upload_wrong_connection)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto upload_reply = client_->Update("item_id", 100, "old_etag");
    wait_for(upload_reply);
    ASSERT_TRUE(upload_reply.isValid()) << upload_reply.error().message().toStdString();
    auto upload_id = upload_reply.argumentAt<0>();

    // Try to finish download using a second connection
    QDBusConnection connection2 = QDBusConnection::connectToBus(dbus_->busAddress(), SECOND_CONNECTION_NAME);
    QDBusConnection::disconnectFromBus(SECOND_CONNECTION_NAME);
    ProviderClient client2(bus_name(), object_path(), connection2);
    auto reply = client2.FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_FALSE(reply.isValid());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_TRUE(reply.error().message().startsWith("No such upload: ")) << reply.error().message().toStdString();
}

TEST_F(ProviderInterfaceTest, tempfile_upload)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("tempfile_item_id", file_contents.size(), "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Write);
    size_t total_written = 0;
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [app, &notifier, &total_written](int fd) {
            ssize_t n_written = write(fd, file_contents.data() + total_written, file_contents.size() - total_written);
            if (n_written < 0)
            {
                // Error writing
                notifier.setEnabled(false);
                app->quit();
            }
            total_written += n_written;
            if (total_written == file_contents.size())
            {
                notifier.setEnabled(false);
                app->quit();
            }
        });
    notifier.setEnabled(true);
    app->exec();
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));

    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("item_id", item.item_id);
}

TEST_F(ProviderInterfaceTest, tempfile_upload_short_write)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("tempfile_item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));
    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("wrong number of bytes written", reply.error().message().toStdString());
}

TEST_F(ProviderInterfaceTest, tempfile_upload_long_write)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("tempfile_item_id", file_contents.size() - 5, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Write);
    size_t total_written = 0;
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [app, &notifier, &total_written](int fd) {
            ssize_t n_written = write(fd, file_contents.data() + total_written, file_contents.size() - total_written);
            if (n_written < 0)
            {
                // Error writing
                notifier.setEnabled(false);
                app->quit();
            }
            total_written += n_written;
            if (total_written == file_contents.size())
            {
                notifier.setEnabled(false);
                app->quit();
            }
        });
    notifier.setEnabled(true);
    app->exec();
    // File descriptor is owned by QDBusUnixFileDescriptor, so using
    // shutdown() to make sure the write channel is closed.
    ASSERT_EQ(0, shutdown(socket.fileDescriptor(), SHUT_WR));

    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("wrong number of bytes written", reply.error().message());
}

TEST_F(ProviderInterfaceTest, tempfile_upload_not_closed)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString upload_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Update("tempfile_item_id", 100, "old_etag");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        upload_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    auto reply = client_->FinishUpload(upload_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("Socket not closed", reply.error().message());
}

TEST_F(ProviderInterfaceTest, download)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString download_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Download("item_id");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        download_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }

    std::string data;
    auto app = QCoreApplication::instance();
    QSocketNotifier notifier(socket.fileDescriptor(), QSocketNotifier::Read);
    QObject::connect(
        &notifier, &QSocketNotifier::activated,
        [&data, app, &notifier](int fd) {
            char buf[1024];
            ssize_t n_read = read(fd, buf, sizeof(buf));
            if (n_read <= 0)
            {
                // Error or end of file
                notifier.setEnabled(false);
                app->quit();
            }
            else
            {
                data += string(buf, n_read);
            }
        });
    notifier.setEnabled(true);
    app->exec();
    auto reply = client_->FinishDownload(download_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    // Also check that we got the expected data from the socket.
    EXPECT_EQ("Hello world", data);
}

TEST_F(ProviderInterfaceTest, download_short_read)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QString download_id;
    QDBusUnixFileDescriptor socket;
    {
        auto reply = client_->Download("item_id");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        download_id = reply.argumentAt<0>();
        socket = reply.argumentAt<1>();
    }
    auto reply = client_->FinishDownload(download_id);
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("Not all data read", reply.error().message());
}

TEST_F(ProviderInterfaceTest, finish_download_unknown)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->FinishDownload("no-such-download");
    wait_for(reply);
    ASSERT_TRUE(reply.isError());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_EQ("No such download: no-such-download", reply.error().message());
}

TEST_F(ProviderInterfaceTest, finish_download_wrong_connection)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto download_reply = client_->Download("item_id");
    wait_for(download_reply);
    ASSERT_TRUE(download_reply.isValid()) << download_reply.error().message().toStdString();
    auto download_id = download_reply.argumentAt<0>();

    // Try to finish download using a second connection
    QDBusConnection connection2 = QDBusConnection::connectToBus(dbus_->busAddress(), SECOND_CONNECTION_NAME);
    QDBusConnection::disconnectFromBus(SECOND_CONNECTION_NAME);
    ProviderClient client2(bus_name(), object_path(), connection2);
    auto reply = client2.FinishDownload(download_id);
    wait_for(reply);
    ASSERT_FALSE(reply.isValid());
    EXPECT_EQ(PROVIDER_ERROR + "LogicException", reply.error().name());
    EXPECT_TRUE(reply.error().message().startsWith("No such download: ")) << reply.error().message().toStdString();
}

TEST_F(ProviderInterfaceTest, cancel_download_on_disconnect)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    QDBusServiceWatcher service_watcher;
    service_watcher.setConnection(*service_connection_);
    service_watcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    QSignalSpy service_spy(
        &service_watcher, &QDBusServiceWatcher::serviceUnregistered);

    QDBusUnixFileDescriptor socket;
    {
        QDBusConnection connection2 = QDBusConnection::connectToBus(dbus_->busAddress(), SECOND_CONNECTION_NAME);
        QDBusConnection::disconnectFromBus(SECOND_CONNECTION_NAME);
        service_watcher.addWatchedService(connection2.baseService());
        ProviderClient client2(bus_name(), object_path(), connection2);
        auto reply = client2.Download("item_id");
        wait_for(reply);
        ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
        // Store socket so it will remain open past the closing of the
        // D-Bus connection.
        socket = reply.argumentAt<1>();
    }

    // Wait until we're sure the fact that connection2 closed has
    // reached the service's connection, and then a little more to
    // ensure it is triggered.
    if (service_spy.count() == 0)
    {
        ASSERT_TRUE(service_spy.wait());
    }
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(100);
    timer.start();
    QSignalSpy timer_spy(&timer, &QTimer::timeout);
    ASSERT_TRUE(timer_spy.wait());
}

TEST_F(ProviderInterfaceTest, delete_)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Delete("item_id");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
}

TEST_F(ProviderInterfaceTest, move)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Move("child_id", "new_parent_id", "New name");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("child_id", item.item_id);
    EXPECT_EQ(QVector<QString>{ "new_parent_id" }, item.parent_ids);
    EXPECT_EQ("New name", item.name);
    EXPECT_EQ(ItemType::file, item.type);
}

TEST_F(ProviderInterfaceTest, copy)
{
    set_provider(unique_ptr<ProviderBase>(new TestProvider));

    auto reply = client_->Copy("child_id", "new_parent_id", "New name");
    wait_for(reply);
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    auto item = reply.value();
    EXPECT_EQ("new_id", item.item_id);
    EXPECT_EQ(QVector<QString>{ "new_parent_id" }, item.parent_ids);
    EXPECT_EQ("New name", item.name);
    EXPECT_EQ(ItemType::file, item.type);
}


int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qDBusRegisterMetaType<unity::storage::internal::ItemMetadata>();
    qDBusRegisterMetaType<QList<unity::storage::internal::ItemMetadata>>();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
