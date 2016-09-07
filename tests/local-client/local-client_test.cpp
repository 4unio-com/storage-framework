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

#include <unity/storage/qt/client/client-api.h>

#include <unity/storage/qt/client/internal/local_client/boost_filesystem.h>
#include <unity/storage/qt/client/internal/local_client/tmpfile_prefix.h>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QFile>
#include <QFutureWatcher>
#include <QSignalSpy>
#include <QTimer>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <glib.h>
#pragma GCC diagnostic pop

#include <fstream>

Q_DECLARE_METATYPE(QLocalSocket::LocalSocketState)

using namespace unity::storage;
using namespace unity::storage::qt::client;
using namespace std;

// Yes, that's ridiculously long, but the builders in Jenkins and the CI Train
// are stupifyingly slow at times.
static constexpr int SIGNAL_WAIT_TIME = 30000;

// Bunch of helper functions to reduce the amount of noise in the tests.

template<typename T>
void wait(T fut)
{
    QFutureWatcher<decltype(fut.result())> w;
    QSignalSpy spy(&w, &decltype(w)::finished);
    w.setFuture(fut);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
}

template<>
void wait(QFuture<void> fut)
{
    QFutureWatcher<void> w;
    QSignalSpy spy(&w, &decltype(w)::finished);
    w.setFuture(fut);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
}

template <typename T>
T call(QFuture<T> fut)
{
    wait(fut);
    return fut.result();
}

template <>
void call(QFuture<void> fut)
{
    wait(fut);
    fut.waitForFinished();
}

Account::SPtr get_account(Runtime::SPtr const& runtime)
{
    auto accounts = call(runtime->accounts());
    return accounts[0];
}

Root::SPtr get_root(Runtime::SPtr const& runtime)
{
    auto acc = get_account(runtime);
    auto roots = call(acc->roots());
    return roots[0];
}

Folder::SPtr get_parent(Item::SPtr const& item)
{
    assert(item->type() != ItemType::root);
    auto parents = call(item->parents());
    return parents[0];
}

void clear_folder(Folder::SPtr folder)
{
    auto items = call(folder->list());
    for (auto i : items)
    {
        i->delete_item().waitForFinished();
    }
}

bool content_matches(File::SPtr const& file, QByteArray const& expected)
{
    QFile f(file->native_identity());
    assert(f.open(QIODevice::ReadOnly));
    QByteArray buf = f.readAll();
    return buf == expected;
}

File::SPtr write_file(Folder::SPtr const& folder, QString const& name, QByteArray const& contents)
{
    QString ofile = folder->native_identity() + "/" + name;
    QFile f(ofile);
    assert(f.open(QIODevice::Truncate | QIODevice::WriteOnly));
    if (!contents.isEmpty())
    {
        assert(f.write(contents));
    }
    f.close();
    auto items = call(folder->lookup(name));
    return dynamic_pointer_cast<File>(items[0]);
}

File::SPtr make_deleted_file(Folder::SPtr parent, QString const& name)
{
    auto file = write_file(parent, name, "bytes");
    call(file->delete_item());
    return file;
}

Folder::SPtr make_deleted_folder(Folder::SPtr parent, QString const& name)
{
    auto folder = call(parent->create_folder(name));
    call(folder->delete_item());
    return folder;
}

TEST(Runtime, lifecycle)
{
    auto runtime = Runtime::create();
    runtime->shutdown();
    runtime->shutdown();  // Just to show that this is safe.
}

TEST(Runtime, basic)
{
    auto runtime = Runtime::create();

    auto acc = get_account(runtime);
    EXPECT_EQ(runtime, acc->runtime());
    auto owner = acc->owner();
    EXPECT_EQ(QString(g_get_user_name()), owner);
    auto owner_id = acc->owner_id();
    EXPECT_EQ(QString::number(getuid()), owner_id);
    auto description = acc->description();
    EXPECT_EQ(description, QString("Account for ") + owner + " (" + owner_id + ")");
}

TEST(Runtime, accounts)
{
    auto runtime = Runtime::create();

    auto acc = get_account(runtime);
    auto roots = call(acc->roots());
    EXPECT_EQ(1, roots.size());

    // Get roots again, to get coverage for lazy initialization.
    roots = call(acc->roots());
    ASSERT_EQ(1, roots.size());
}

TEST(Root, basic)
{
    auto runtime = Runtime::create();

    auto acc = get_account(runtime);
    auto root = get_root(runtime);
    EXPECT_EQ(acc, root->account());
    EXPECT_EQ(ItemType::root, root->type());
    EXPECT_EQ("", root->name());
    EXPECT_NE("", root->etag());

    {
        auto parents = call(root->parents());
        EXPECT_TRUE(parents.isEmpty());
        EXPECT_TRUE(root->parent_ids().isEmpty());
    }

    {
        // get(<root-path>) must return the root.
        auto item = call(root->get(root->native_identity()));
        EXPECT_NE(nullptr, dynamic_pointer_cast<Root>(item));
        EXPECT_TRUE(root->equal_to(item));
    }

    // Free and used space can be anything, but must be > 0.
    {
        auto free_space = call(root->free_space_bytes());
        cerr << "bytes free: " << free_space << endl;
        EXPECT_GT(free_space, 0);
    }

    {
        auto used_space = call(root->used_space_bytes());
        cerr << "bytes used: " << used_space << endl;
        EXPECT_GT(used_space, 0);
    }
}

TEST(Folder, basic)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto items = call(root->list());
    EXPECT_TRUE(items.isEmpty());

    // Create a file and check that it was created with correct type, name, and size 0.
    auto uploader = call(root->create_file("file1", 0));
    auto file = call(uploader->finish_upload());
    EXPECT_EQ(ItemType::file, file->type());
    EXPECT_EQ("file1", file->name());
    EXPECT_EQ(0, file->size());
    EXPECT_EQ(root->native_identity() + "/file1", file->native_identity());

    // Create a folder and check that it was created with correct type and name.
    auto folder = call(root->create_folder("folder1"));
    EXPECT_EQ(ItemType::folder, folder->type());
    EXPECT_EQ("folder1", folder->name());
    EXPECT_EQ(root->native_identity() + "/folder1", folder->native_identity());

    // Check that we can find both file1 and folder1.
    auto item = call(root->lookup("file1"))[0];
    file = dynamic_pointer_cast<File>(item);
    ASSERT_NE(nullptr, file);
    EXPECT_EQ("file1", file->name());
    EXPECT_EQ(0, file->size());

    item = call(root->lookup("folder1"))[0];
    folder = dynamic_pointer_cast<Folder>(item);
    ASSERT_NE(nullptr, folder);
    ASSERT_EQ(nullptr, dynamic_pointer_cast<Root>(folder));
    EXPECT_EQ("folder1", folder->name());

    item = call(root->get(file->native_identity()));
    file = dynamic_pointer_cast<File>(item);
    ASSERT_NE(nullptr, file);
    EXPECT_EQ("file1", file->name());
    EXPECT_EQ(0, file->size());

    item = call(root->get(folder->native_identity()));
    folder = dynamic_pointer_cast<Folder>(item);
    ASSERT_NE(nullptr, folder);
    ASSERT_EQ(nullptr, dynamic_pointer_cast<Root>(folder));
    EXPECT_EQ("folder1", folder->name());

    // Check that list() returns file1 and folder1.
    items = root->list();
    ASSERT_EQ(2, items.size());
    auto left = items[0];
    auto right = items[1];
    ASSERT_TRUE((dynamic_pointer_cast<File>(left) && dynamic_pointer_cast<Folder>(right))
                ||
                (dynamic_pointer_cast<File>(right) && dynamic_pointer_cast<Folder>(left)));
    if (dynamic_pointer_cast<File>(left))
    {
        file = dynamic_pointer_cast<File>(left);
        folder = dynamic_pointer_cast<Folder>(right);
    }
    else
    {
        file = dynamic_pointer_cast<File>(right);
        folder = dynamic_pointer_cast<Folder>(left);
    }
    EXPECT_EQ("file1", file->name());
    EXPECT_EQ("folder1", folder->name());
    EXPECT_TRUE(file->root()->equal_to(root));
    EXPECT_TRUE(folder->root()->equal_to(root));

    // Parent of both file and folder must be the root.
    EXPECT_TRUE(root->equal_to(get_parent(file)));
    EXPECT_TRUE(root->equal_to(get_parent(folder)));
    EXPECT_EQ(root->native_identity(), file->parent_ids()[0]);
    EXPECT_EQ(root->native_identity(), folder->parent_ids()[0]);

    // Delete the file and check that only the directory is left.
    call(file->delete_item());
    items = call(root->list());
    ASSERT_EQ(1, items.size());
    folder = dynamic_pointer_cast<Folder>(items[0]);
    ASSERT_NE(nullptr, folder);
    EXPECT_EQ("folder1", folder->name());;

    // Delete the folder and check that the root is empty.
    folder->delete_item().waitForFinished();
    items = call(root->list());
    ASSERT_EQ(0, items.size());
}

TEST(Folder, nested)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto d1 = call(root->create_folder("d1"));
    auto d2 = call(d1->create_folder("d2"));

    // Parent of d2 must be d1.
    EXPECT_TRUE(get_parent(d2)->equal_to(d1));
    EXPECT_TRUE(d2->parent_ids()[0] == d1->native_identity());

    // Delete is recursive
    d1->delete_item().waitForFinished();
    auto items = call(root->list());
    ASSERT_EQ(0, items.size());
}

TEST(File, upload)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    {
        // Upload a few bytes.
        QByteArray const contents = "Hello\n";
        auto uploader = call(root->create_file("new_file", contents.size()));
        auto written = uploader->socket()->write(contents);
        ASSERT_EQ(contents.size(), written);

        auto file = call(uploader->finish_upload());
        EXPECT_EQ(contents.size(), file->size());
        ASSERT_TRUE(content_matches(file, contents));

        // Calling finish_upload() more than once must return the original future.
        auto file2 = call(uploader->finish_upload());
        EXPECT_TRUE(file2->equal_to(file));

        // Calling cancel() after finish_upload must do nothing.
        uploader->cancel();
        file2 = call(uploader->finish_upload());
        EXPECT_TRUE(file2->equal_to(file));

        call(file->delete_item());
    }

    {
        // Upload exactly 64 KB.
        QByteArray const contents(64 * 1024, 'a');
        auto uploader = call(root->create_file("new_file", contents.size()));
        auto written = uploader->socket()->write(contents);
        ASSERT_EQ(contents.size(), written);

        auto file = call(uploader->finish_upload());
        EXPECT_EQ(contents.size(), file->size());
        ASSERT_TRUE(content_matches(file, contents));

        call(file->delete_item());
    }

    {
        // Upload 1000 KBj
        QByteArray const contents(1000 * 1024, 'a');
        auto uploader = call(root->create_file("new_file", contents.size()));
        auto written = uploader->socket()->write(contents);
        ASSERT_EQ(contents.size(), written);

        auto file = call(uploader->finish_upload());
        EXPECT_EQ(contents.size(), file->size());
        ASSERT_TRUE(content_matches(file, contents));

        call(file->delete_item());
    }

    {
        // Upload empty file.
        auto uploader = call(root->create_file("new_file", 0));
        auto file = call(uploader->finish_upload());
        ASSERT_EQ(0, file->size());

        // Again, and check that the ETag is different.
        auto old_etag = file->etag();
        sleep(1);
        uploader = call(file->create_uploader(ConflictPolicy::overwrite, 0));
        file = call(uploader->finish_upload());
        EXPECT_NE(old_etag, file->etag());

        call(file->delete_item());
    }

    {
        // Let the uploader go out of scope and check that the file was not created.
        call(root->create_file("new_file", 0));
        boost::filesystem::path path(TEST_DIR "/storage-framework/new_file");
        auto status = boost::filesystem::status(path);
        ASSERT_FALSE(boost::filesystem::exists(status));
    }
}

TEST(File, create_uploader)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Make a new file first.
    auto uploader = call(root->create_file("new_file", 0));
    EXPECT_EQ(0, uploader->size());
    auto file = call(uploader->finish_upload());
    EXPECT_EQ(0, file->size());
    auto old_etag = file->etag();

    // Create uploader for the file and write nothing.
    uploader = call(file->create_uploader(ConflictPolicy::overwrite, 0));
    file = call(uploader->finish_upload());
    EXPECT_EQ(0, file->size());

    // Same test again, but this time, we write a bunch of data.
    std::string s(1000000, 'a');
    uploader = call(file->create_uploader(ConflictPolicy::overwrite, s.size()));
    EXPECT_EQ(1000000, uploader->size());
    uploader->socket()->write(&s[0], s.size());
    uploader->socket()->waitForBytesWritten(SIGNAL_WAIT_TIME);

    // Need to sleep here, otherwise it is possible for the
    // upload to finish within the granularity of the file system time stamps.
    sleep(1);
    file = call(uploader->finish_upload());
    EXPECT_EQ(1000000, file->size());
    EXPECT_NE(old_etag, file->etag());

    file->delete_item().waitForFinished();
}

TEST(File, cancel_upload)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    {
        auto uploader = call(root->create_file("new_file", 20));

        // We haven't called finish_upload(), so the cancel is guaranteed
        // to catch the uploader in the in_progress state.
        uploader->cancel();
        EXPECT_THROW(call(uploader->finish_upload()), CancelledException);

        boost::filesystem::path path(TEST_DIR "/storage-framework/new_file");
        auto status = boost::filesystem::status(path);
        ASSERT_FALSE(boost::filesystem::exists(status));
    }

    {
        // Create a file with a few bytes.
        QByteArray original_contents = "Hello World!\n";
        auto file = write_file(root, "new_file", original_contents);

        // Create an uploader for the file and write a bunch of bytes.
        auto uploader = call(file->create_uploader(ConflictPolicy::overwrite, original_contents.size()));
        QByteArray const contents(1024 * 1024, 'a');
        auto written = uploader->socket()->write(contents);
        ASSERT_EQ(contents.size(), written);

        // No finish_upload() here, so the transfer is still in progress. Now cancel.
        uploader->cancel();

        // finish_upload() must indicate that the upload was cancelled.
        EXPECT_THROW(call(uploader->finish_upload()), CancelledException);

        // The original file contents must still be intact.
        EXPECT_EQ(original_contents.size(), file->size());
        ASSERT_TRUE(content_matches(file, original_contents));

        call(file->delete_item());
    }
}

TEST(File, upload_conflict)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Make a new file on disk.
    QByteArray const contents = "";
    auto file = write_file(root, "new_file", contents);
    auto uploader = call(file->create_uploader(ConflictPolicy::error_if_conflict, contents.size()));

    // Touch the file on disk to give it a new time stamp.
    sleep(1);
    ASSERT_EQ(0, system((string("touch ") + file->native_identity().toStdString()).c_str()));

    try
    {
        // Must get an exception because the time stamps no longer match.
        call(uploader->finish_upload());
        FAIL();
    }
    catch (ConflictException const&)
    {
        // TODO: check exception details.
    }

    call(file->delete_item());
}

TEST(File, upload_error)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto uploader = call(root->create_file("new_file", 0));
    // Make new_file, so it gets in the way during finish_upload().
    write_file(root, "new_file", "");

    try
    {
        call(uploader->finish_upload());
        FAIL();
    }
    catch (ExistsException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Uploader::finish_upload(): item with name \""));
        EXPECT_TRUE(e.error_message().endsWith("\" exists already"));
        EXPECT_EQ(TEST_DIR "/storage-framework/new_file", e.native_identity()) << e.native_identity().toStdString();
        EXPECT_EQ("new_file", e.name());
    }
}

TEST(File, upload_bad_size)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Uploader expects 100 bytes, but we write only 50.
    {
        auto uploader = call(root->create_file("file50", 100));
        auto socket = uploader->socket();

        QByteArray const contents(50, 'x');
        auto written = socket->write(contents);
        ASSERT_EQ(50, written);

        try
        {
            call(uploader->finish_upload());
            FAIL();
        }
        catch (LogicException const& e)
        {
            EXPECT_TRUE(e.error_message().startsWith("Uploader::finish_upload(): "));
            EXPECT_TRUE(e.error_message().endsWith(": upload size of 100 does not match actual number of bytes read: 50"));
        }
    }

    // Uploader expects 100 bytes, but we write 101.
    {
        auto uploader = call(root->create_file("file100", 100));
        auto socket = uploader->socket();

        QByteArray const contents(101, 'x');
        auto written = socket->write(contents);
        ASSERT_EQ(101, written);

        try
        {
            call(uploader->finish_upload());
            FAIL();
        }
        catch (LogicException const& e)
        {
            EXPECT_TRUE(e.error_message().startsWith("Uploader::finish_upload(): "));
            EXPECT_TRUE(e.error_message().endsWith(": upload size of 100 does not match actual number of bytes read: 101"));
        }

        // Calling finish_upload() again must return the same future as the first time.
        try
        {
            call(uploader->finish_upload());
            FAIL();
        }
        catch (LogicException const& e)
        {
            EXPECT_TRUE(e.error_message().startsWith("Uploader::finish_upload(): "));
            EXPECT_TRUE(e.error_message().endsWith(": upload size of 100 does not match actual number of bytes read: 101"));
        }
    }
}

TEST(File, create_uploader_bad_arg)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto file = write_file(root, "new_file", 0);
    try
    {
        call(file->create_uploader(ConflictPolicy::overwrite, -1));
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("File::create_uploader(): size must be >= 0", e.error_message());
    }
}

TEST(File, download)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    {
        // Download a few bytes.
        QByteArray const contents = "Hello\n";
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();
        QByteArray buf;
        do
        {
            // Need to pump the event loop while the socket does its thing.
            QSignalSpy spy(socket.get(), &QIODevice::readyRead);
            auto bytes_to_read = socket->bytesAvailable();
            if (bytes_to_read == 0)
            {
                ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
            }
            buf.append(socket->read(bytes_to_read));
        } while (buf.size() < contents.size());

        // Wait for disconnected signal.
        QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        ASSERT_NO_THROW(call(downloader->finish_download()));

        // Contents must match.
        EXPECT_EQ(contents, buf);
    }

    {
        // Download exactly 64 KB.
        QByteArray const contents(64 * 1024, 'a');
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();
        QByteArray buf;
        do
        {
            // Need to pump the event loop while the socket does its thing.
            QSignalSpy spy(socket.get(), &QIODevice::readyRead);
            auto bytes_to_read = socket->bytesAvailable();
            if (bytes_to_read == 0)
            {
                ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
            }
            buf.append(socket->read(bytes_to_read));
        } while (buf.size() < contents.size());

        // Wait for disconnected signal.
        QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        ASSERT_NO_THROW(call(downloader->finish_download()));

        // Contents must match
        EXPECT_EQ(contents, buf);
    }

    {
        // Download 1 MB + 1 bytes.
        QByteArray const contents(1024 * 1024 + 1, 'a');
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();
        QByteArray buf;
        do
        {
            // Need to pump the event loop while the socket does its thing.
            QSignalSpy spy(socket.get(), &QIODevice::readyRead);
            auto bytes_to_read = socket->bytesAvailable();
            if (bytes_to_read == 0)
            {
                ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
            }
            buf.append(socket->read(bytes_to_read));
        } while (buf.size() < contents.size());

        // Wait for disconnected signal.
        QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        ASSERT_NO_THROW(call(downloader->finish_download()));

        // Contents must match
        EXPECT_EQ(contents, buf);
    }

    {
        // Download file containing zero bytes
        QByteArray const contents;
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();

        // No readyRead ever arrives in this case, just wait for disconnected.
        QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
        if (socket->state() != QLocalSocket::UnconnectedState)
        {
            ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
        }

        ASSERT_NO_THROW(call(downloader->finish_download()));
    }

    {
        // Don't ever call read on empty file.
        QByteArray const contents;
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();

        // No readyRead ever arrives in this case, just wait for disconnected.
        QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
        if (socket->state() != QLocalSocket::UnconnectedState)
        {
            ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
        }

        // This succeeds because the provider disconnects as soon
        // as it realizes that there is nothing to write.
        ASSERT_NO_THROW(call(downloader->finish_download()));
    }

    {
        // Don't ever call read on small file.
        QByteArray const contents("some contents");
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();

        // Wait for disconnected.
        if (socket->state() != QLocalSocket::UnconnectedState)
        {
            QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
            ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
        }

        // This succeeds because the provider has written everything and disconnected.
        ASSERT_NO_THROW(call(downloader->finish_download()));
    }

    {
        // Don't ever call read on large file.
        QByteArray const contents(1024 * 1024, 'a');
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
        EXPECT_TRUE(file->equal_to(downloader->file()));

        auto socket = downloader->socket();

        // Wait for first readyRead. Not all data fits into the socket buffer.
        QSignalSpy spy(socket.get(), &QLocalSocket::readyRead);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        // This fails because the provider still has data left to write.
        try
        {
            call(downloader->finish_download());
            FAIL();
        }
        catch (StorageException const& e)
        {
            // TODO: check exception details
        }
    }

    {
        // Let downloader go out of scope.
        QByteArray const contents(1024 * 1024, 'a');
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());
    }

    {
        // Let downloader future go out of scope.
        QByteArray const contents(1024 * 1024, 'a');
        auto file = write_file(root, "file", contents);

        file->create_downloader();
    }
}

TEST(File, cancel_download)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    {
        // Download enough bytes to prevent a single write in the provider from completing the download.
        QByteArray const contents(1024 * 1024, 'a');
        auto file = write_file(root, "file", contents);

        auto downloader = call(file->create_downloader());

        // We haven't read anything, so the cancel is guaranteed to catch the
        // downloader in the in_progress state.
        downloader->cancel();
        ASSERT_THROW(call(downloader->finish_download()), CancelledException);
    }

    {
        // Download a few bytes.
        QByteArray const contents = "Hello\n";
        auto file = write_file(root, "file", contents);

        // Finish the download.
        auto downloader = call(file->create_downloader());
        auto socket = downloader->socket();
        QByteArray buf;
        do
        {
            // Need to pump the event loop while the socket does its thing.
            QSignalSpy spy(socket.get(), &QIODevice::readyRead);
            auto bytes_to_read = socket->bytesAvailable();
            if (bytes_to_read == 0)
            {
                ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
            }
            buf.append(socket->read(bytes_to_read));
        } while (buf.size() < contents.size());

        // Wait for disconnected signal.
        if (socket->state() != QLocalSocket::UnconnectedState)
        {
            QSignalSpy spy(socket.get(), &QLocalSocket::disconnected);
            ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
        }

        // Now send the cancel. The download is finished already, and the cancel
        // is too late, so finish_download() must report that the download
        // worked OK.
        downloader->cancel();
        ASSERT_NO_THROW(call(downloader->finish_download()));
    }
}

TEST(File, download_error)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    QByteArray const contents(1024 * 1024, 'a');
    auto file = write_file(root, "file", contents);

    auto downloader = call(file->create_downloader());
    EXPECT_TRUE(file->equal_to(downloader->file()));

    auto socket = downloader->socket();

    {
        // Wait for first readyRead. Not all data fits into the socket buffer.
        QSignalSpy spy(socket.get(), &QLocalSocket::readyRead);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    }

    {
        // Now close the socket, to force an error at the writing end.
        // This gives us coverage of the error handling logic in the download worker.
        socket->abort();
        // Wait a little, to give the worker a chance to notice the problem.
        // We don't wait for a signal here because all attempts to disable the
        // socket (via close(), abort(), or disconnectFromServer() also
        // stop the stateChanged signal from arriving.
        QTimer timer;
        QSignalSpy spy(&timer, &QTimer::timeout);
        timer.start(1000);
        spy.wait();
    }

    try
    {
        call(downloader->finish_download());
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Downloader: QLocalSocket: "));
    }
}

TEST(File, size_error)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto file = write_file(root, "file", "");

    ASSERT_EQ(0, system("rm " TEST_DIR "/storage-framework/file"));
    EXPECT_THROW(file->size(), ResourceException);
}

TEST(Item, move)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Check that rename works within the same folder.
    QByteArray const contents = "Hello\n";
    auto f1 = write_file(root, "f1", contents);
    auto f2 = call(f1->move(root, "f2"));

    // File must be found under new name.
    auto items = call(root->list());
    ASSERT_EQ(1, items.size());
    f2 = dynamic_pointer_cast<File>(items[0]);
    ASSERT_FALSE(f2 == nullptr);

    // Make a folder and move f2 into it.
    auto folder = call(root->create_folder("folder"));
    f2 = call(f2->move(folder, "f2"));
    EXPECT_TRUE(get_parent(f2)->equal_to(folder));

    // Move the folder
    auto item = call(folder->move(root, "folder2"));
    folder = dynamic_pointer_cast<Folder>(item);
    EXPECT_EQ("folder2", folder->name());
}

TEST(Item, copy)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    QByteArray const contents = "hello\n";
    auto item = write_file(root, "file", contents);
    auto copied_item = call(item->copy(root, "copy_of_file"));
    EXPECT_EQ("copy_of_file", copied_item->name());
    File::SPtr copied_file = dynamic_pointer_cast<File>(item);
    ASSERT_NE(nullptr, copied_file);
    EXPECT_TRUE(content_matches(copied_file, contents));
}

TEST(Item, recursive_copy)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Create the following structure:
    // folder
    // folder/empty_folder
    // folder/non_empty_folder
    // folder/non_empty_folder/nested_file
    // folder/non_empty_folder/<TMPFILE_PREFIX>1234-1234-1234-1234
    // folder/file
    // folder/<TMPFILE_PREFIX>1234-1234-1234-1234

    string root_path = root->native_identity().toStdString();
    ASSERT_EQ(0, mkdir((root_path + "/folder").c_str(), 0700));
    ASSERT_EQ(0, mkdir((root_path + "/folder/empty_folder").c_str(), 0700));
    ASSERT_EQ(0, mkdir((root_path + "/folder/non_empty_folder").c_str(), 0700));
    ofstream(root_path + "/folder/non_empty_folder/nested_file");
    ofstream(root_path + "/folder/file");

    // Add dirs that look like a tmp dirs, to get coverage on skipping those.
    ASSERT_EQ(0, mkdir((root_path + "/folder/" + TMPFILE_PREFIX "1234-1234-1234-1234").c_str(), 0700));
    ASSERT_EQ(0, mkdir((root_path + "/folder/non_empty_folder/" + TMPFILE_PREFIX "1234-1234-1234-1234").c_str(), 0700));
    // Copy folder to folder2
    auto folder = dynamic_pointer_cast<Folder>(call(root->lookup("folder"))[0]);
    ASSERT_NE(nullptr, folder);
    auto item = call(folder->copy(root, "folder2"));

    // Verify that folder2 now contains the same structure as folder.
    auto folder2 = dynamic_pointer_cast<Folder>(item);
    ASSERT_NE(nullptr, folder2);
    EXPECT_NO_THROW(call(folder2->lookup("empty_folder"))[0]);
    item = call(folder2->lookup("non_empty_folder"))[0];
    auto non_empty_folder = dynamic_pointer_cast<Folder>(item);
    ASSERT_NE(nullptr, non_empty_folder);
    EXPECT_NO_THROW(call(non_empty_folder->lookup("nested_file"))[0]);
    EXPECT_NO_THROW(call(folder2->lookup("file"))[0]);
}

TEST(Item, time)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    auto now = QDateTime::currentDateTimeUtc();
    sleep(1);
    auto uploader = call(root->create_file("file", 0));
    auto file = call(uploader->finish_upload());
    auto t = file->last_modified_time();
    // Rough check that the time is sane.
    EXPECT_LE(now, t);
    EXPECT_LE(t, now.addSecs(5));

    auto creation_time = file->creation_time();
    EXPECT_FALSE(creation_time.isValid());
}

TEST(Item, comparison)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    // Create two files.
    auto uploader = call(root->create_file("file1", 0));
    auto file1 = call(uploader->finish_upload());

    uploader = call(root->create_file("file2", 0));
    auto file2 = call(uploader->finish_upload());

    EXPECT_FALSE(file1->equal_to(file2));

    // Retrieve file1 via lookup, so we get a different proxy.
    auto item = call(root->lookup("file1"))[0];
    auto other_file1 = dynamic_pointer_cast<File>(item);
    EXPECT_NE(file1, other_file1);              // Compares shared_ptr values
    EXPECT_TRUE(file1->equal_to(other_file1));  // Deep comparison

    // Comparing against a deleted file must return false.
    call(file1->delete_item());
    EXPECT_FALSE(file1->equal_to(file2));
    EXPECT_FALSE(file2->equal_to(file1));

    // Delete file2 as well and compare again.
    call(file2->delete_item());
    EXPECT_FALSE(file1->equal_to(file2));
}

TEST(Item, exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        call(root->copy(nullptr, "new name"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Item::copy(): new_parent cannot be nullptr", e.error_message());
    }

    auto file = write_file(root, "file", 0);

    try
    {
        call(file->copy(root, TMPFILE_PREFIX "copy_of_file"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Item::copy(): names beginning with \".storage-framework-\" are reserved", e.error_message());
    }

    try
    {
        call(file->copy(root, file->name()));
        FAIL();
    }
    catch (ExistsException const& e)
    {
        EXPECT_EQ("Item::copy(): item with name \"file\" exists already", e.error_message());
        EXPECT_EQ("file", e.name());
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        auto file = write_file(root, "file", "");
        ASSERT_EQ(0, unlink(file->native_identity().toStdString().c_str()));

        call(file->copy(root, file->name()));
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::copy(): "));
    }

    try
    {
        auto file = write_file(root, "file", "");
        ASSERT_EQ(0, unlink(file->native_identity().toStdString().c_str()));

        call(file->move(root, "new_name"));
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::move(): "));
    }

    try
    {
        auto file = write_file(root, "file", "");
        ASSERT_EQ(0, unlink(file->native_identity().toStdString().c_str()));
        ASSERT_EQ(0, system("chmod -x " TEST_DIR "/storage-framework"));

        call(file->delete_item());
        FAIL();
    }
    catch (PermissionException const& e)
    {
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
        EXPECT_TRUE(e.error_message().startsWith("Item::delete_item(): "));
    }
    catch (std::exception const&)
    {
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
        FAIL();
    }
    ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));

    try
    {
        call(root->move(nullptr, "new name"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Item::move(): new_parent cannot be nullptr", e.error_message());
    }

    try
    {
        call(root->move(root, "new name"));
        FAIL();
    }
    catch (LogicException const& e)
    {
        EXPECT_EQ("Item::move(): cannot move root folder", e.error_message());
    }

    try
    {
        auto file = write_file(root, "file", "");
        call(file->move(root, file->name()));
        FAIL();
    }
    catch (ExistsException const& e)
    {
        EXPECT_EQ("Item::move(): item with name \"file\" exists already", e.error_message());
        EXPECT_EQ("file", e.name());
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        call(file->move(root, TMPFILE_PREFIX "copy_of_file"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Item::move(): names beginning with \".storage-framework-\" are reserved", e.error_message());
    }

    try
    {
        call(root->lookup("abc/def"));
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::lookup(): name \"abc/def\" contains more than one path component",
                  e.error_message()) << e.what();
    }

    try
    {
        call(root->create_folder(".."));
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::create_folder(): invalid name: \"..\"", e.error_message()) << e.what();
    }
}

TEST(Folder, exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        write_file(root, TMPFILE_PREFIX "file", "");
        call(root->lookup(TMPFILE_PREFIX "file"));
        FAIL();
    }
    catch (NotExistsException const& e)
    {
        string cmd = "rm ";
        cmd += string(TEST_DIR) + "/storage-framework/" + TMPFILE_PREFIX "file";
        ASSERT_EQ(0, system(cmd.c_str()));
        EXPECT_EQ("Folder::lookup(): no such item: \".storage-framework-file\"", e.error_message());
        EXPECT_EQ(".storage-framework-file", e.key());
    }

    {
        auto fifo_id = root->native_identity() + "/fifo";
        string cmd = "mkfifo " + fifo_id.toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));

        try
        {
            call(root->lookup("fifo"));
            FAIL();
        }
        catch (NotExistsException const& e)
        {
            EXPECT_EQ("Folder::lookup(): no such item: \"fifo\"", e.error_message()) << e.what();
            EXPECT_EQ("fifo", e.key());
        }

        cmd = "rm " + fifo_id.toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));
    }

    try
    {
        call(root->lookup("no_such_file"));
        FAIL();
    }
    catch (NotExistsException const& e)
    {
        EXPECT_EQ("Folder::lookup(): no such item: \"no_such_file\"", e.error_message());
        EXPECT_EQ("no_such_file", e.key());
    }

    try
    {
        call(root->create_folder(TMPFILE_PREFIX "folder"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::create_folder(): names beginning with \".storage-framework-\" are reserved", e.error_message());
    }

    try
    {
        EXPECT_NO_THROW(call(root->create_folder("folder")));
        call(root->create_folder("folder"));
        FAIL();
    }
    catch (ExistsException const& e)
    {
        EXPECT_EQ("Folder::create_folder(): item with name \"folder\" exists already", e.error_message());
        EXPECT_EQ("folder", e.name());
        EXPECT_EQ(TEST_DIR "/storage-framework/folder", e.native_identity());
    }

    try
    {
        call(root->create_file("new_file", -1));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::create_file(): size must be >= 0", e.error_message());
    }

    try
    {
        call(root->create_file(TMPFILE_PREFIX "new_file", 0));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::create_file(): names beginning with \".storage-framework-\" are reserved", e.error_message());
    }

    try
    {
        write_file(root, "file", "");
        call(root->create_file("file", 0));
        FAIL();
    }
    catch (ExistsException const& e)
    {
        EXPECT_EQ("Folder::create_file(): item with name \"file\" exists already", e.error_message());
        EXPECT_EQ("file", e.name());
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    ASSERT_EQ(0, system("chmod -x " TEST_DIR "/storage-framework"));
    try
    {
        call(root->create_file("new_file", 0));
        FAIL();
    }
    catch (PermissionException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::create_file(): "));
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
    }
    catch (std::exception const& e)
    {
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
        FAIL();
    }
    ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));

    ASSERT_EQ(0, system("chmod -x " TEST_DIR "/storage-framework"));
    try
    {
        call(root->create_folder("new_folder"));
        FAIL();
    }
    catch (PermissionException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::create_folder(): "));
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
    }
    catch (std::exception const& e)
    {
        ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));
        FAIL();
    }
    ASSERT_EQ(0, system("chmod +x " TEST_DIR "/storage-framework"));

    try
    {
        call(root->create_file("new_file", -1));
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Folder::create_file(): size must be >= 0", e.error_message());
    }
}

TEST(Root, root_exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        call(root->delete_item());
        FAIL();
    }
    catch (LogicException const& e)
    {
        EXPECT_EQ("Item::delete_item(): cannot delete root folder", e.error_message());
    }

    try
    {
        call(root->get("abc"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Root::get(): identity \"abc\" must be an absolute path", e.error_message());
    }

    try
    {
        call(root->get("/etc"));
        FAIL();
    }
    catch (InvalidArgumentException const& e)
    {
        EXPECT_EQ("Root::get(): identity \"/etc\" points outside the root folder", e.error_message());
    }

    {
        auto folder = call(root->create_folder("folder"));
        auto file = write_file(folder, "testfile", "hello");

        // Remove permission from folder.
        string cmd = "chmod -x " + folder->native_identity().toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));

        try
        {
            file = dynamic_pointer_cast<File>(call(root->get(file->native_identity())));
            FAIL();
        }
        catch (PermissionException const& e)
        {
            EXPECT_TRUE(e.error_message().startsWith("Root::get(): "));
            EXPECT_TRUE(e.error_message().contains("Permission denied"));
        }
        catch (...)
        {
            cmd = "chmod +x " + folder->native_identity().toStdString();
            ASSERT_EQ(0, system(cmd.c_str()));
        }

        cmd = "chmod +x " + folder->native_identity().toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));

        clear_folder(root);
    }

    {
        auto file = write_file(root, "testfile", "hello");

        QString id = file->native_identity();
        id.append("_doesnt_exist");

        try
        {
            file = dynamic_pointer_cast<File>(call(root->get(id)));
            FAIL();
        }
        catch (NotExistsException const& e)
        {
            EXPECT_EQ(id, e.key());
        }

        clear_folder(root);
    }

    {
        auto fifo_id = root->native_identity() + "/fifo";
        string cmd = "mkfifo " + fifo_id.toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));

        try
        {
            call(root->get(fifo_id));
            FAIL();
        }
        catch (NotExistsException const& e)
        {
            EXPECT_EQ(fifo_id, e.key());
        }

        cmd = "rm " + fifo_id.toStdString();
        ASSERT_EQ(0, system(cmd.c_str()));
    }

    {
        string reserved_name = TMPFILE_PREFIX "somefile";
        string full_path = string(TEST_DIR) + "/storage-framework/" + reserved_name;
        string cmd = "touch ";
        cmd += full_path;
        ASSERT_EQ(0, system(cmd.c_str()));

        auto reserved_id = QString::fromStdString(full_path);
        try
        {
            call(root->get(reserved_id));
            FAIL();
        }
        catch (NotExistsException const& e)
        {
            EXPECT_EQ(reserved_id, e.key());
        }

        clear_folder(root);
    }
}

TEST(Item, deleted_exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        auto file = make_deleted_file(root, "file");
        file->etag();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::etag(): "));
        EXPECT_TRUE(e.error_message().endsWith(" was deleted previously"));
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        file->metadata();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::metadata(): "));
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        file->last_modified_time();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::last_modified_time(): "));
    }

    try
    {
        // Copying deleted file must fail.
        auto file = make_deleted_file(root, "file");
        call(file->copy(root, "copy_of_file"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::copy(): "));
    }

    try
    {
        // Copying file into deleted folder must fail.

        // Make target folder.
        auto folder = call(root->create_folder("folder"));

        // Make a file in the root.
        auto uploader = call(root->create_file("file", 0));
        auto file = call(uploader->finish_upload());

        // Delete folder.
        call(folder->delete_item());

        call(file->copy(folder, "file"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::copy(): "));
    }
    clear_folder(root);

    try
    {
        // Moving deleted file must fail.
        auto file = make_deleted_file(root, "file");
        call(file->move(root, "moved_file"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::move(): "));
    }

    try
    {
        // Moving file into deleted folder must fail.

        // Make target folder.
        auto folder = call(root->create_folder("folder"));

        // Make a file in the root.
        auto uploader = call(root->create_file("file", 0));
        auto file = call(uploader->finish_upload());

        // Delete folder.
        call(folder->delete_item());

        call(file->move(folder, "file"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::move(): "));
    }
    clear_folder(root);

    try
    {
        auto file = make_deleted_file(root, "file");
        call(file->parents());
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::parents(): "));
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        file->parent_ids();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::parent_ids(): "));
    }

    try
    {
        // Deleting a deleted item must fail.
        auto file = make_deleted_file(root, "file");
        call(file->delete_item());
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::delete_item(): "));
    }
}

TEST(Folder, deleted_exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        folder->name();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::name(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        call(folder->list());
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::list(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        call(folder->lookup("something"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::lookup(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        call(folder->list());
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::list(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        call(folder->create_folder("nested_folder"));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::create_folder(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        call(folder->create_file("nested_file", 0));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Folder::create_file(): "));
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        folder->creation_time();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::creation_time(): ")) << e.what();
    }

    try
    {
        auto folder = make_deleted_folder(root, "folder");
        folder->native_metadata();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("Item::native_metadata(): ")) << e.what();
    }
}

TEST(File, deleted_exceptions)
{
    auto runtime = Runtime::create();

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        auto file = make_deleted_file(root, "file");
        file->name();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("File::name(): "));
        EXPECT_TRUE(e.error_message().endsWith(" was deleted previously"));
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        file->size();
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("File::size(): "));
        EXPECT_TRUE(e.error_message().endsWith(" was deleted previously"));
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        call(file->create_uploader(ConflictPolicy::overwrite, 0));
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("File::create_uploader(): "));
        EXPECT_TRUE(e.error_message().endsWith(" was deleted previously"));
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }

    try
    {
        auto file = make_deleted_file(root, "file");
        call(file->create_downloader());
        FAIL();
    }
    catch (DeletedException const& e)
    {
        EXPECT_TRUE(e.error_message().startsWith("File::create_downloader(): "));
        EXPECT_TRUE(e.error_message().endsWith(" was deleted previously"));
        EXPECT_EQ(TEST_DIR "/storage-framework/file", e.native_identity());
    }
}

TEST(Runtime, runtime_destroyed_exceptions)
{
    // Gettting an account after shutting down the runtime must fail.
    {
        auto runtime = Runtime::create();
        auto acc = get_account(runtime);
        runtime->shutdown();
        try
        {
            acc->runtime();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Account::runtime(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting an account after destroying the runtime must fail.
    {
        auto runtime = Runtime::create();
        auto acc = get_account(runtime);
        runtime.reset();
        try
        {
            acc->runtime();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Account::runtime(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting accounts after shutting down the runtime must fail.
    {
        auto runtime = Runtime::create();
        runtime->shutdown();
        try
        {
            call(runtime->accounts());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Runtime::accounts(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting the account from a root with a destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        runtime.reset();
        try
        {
            root->account();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Root::account(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting the account from a root with a destroyed account must fail.
    {
        auto runtime = Runtime::create();
        auto acc = get_account(runtime);
        auto root = get_root(runtime);
        runtime.reset();
        acc.reset();
        try
        {
            root->account();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Root::account(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting the root from an item with a destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime.reset();
        try
        {
            file->root();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::root(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting the root from an item with a destroyed root must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime.reset();
        root.reset();
        try
        {
            file->root();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::root(): runtime was destroyed previously", e.error_message());
        }
    }

    // etag() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->etag();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::etag(): runtime was destroyed previously", e.error_message());
        }
    }

    // metadata() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->metadata();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::metadata(): runtime was destroyed previously", e.error_message());
        }
    }

    // last_modified_time() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->last_modified_time();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::last_modified_time(): runtime was destroyed previously", e.error_message());
        }
    }

    // copy() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->copy(root, "file2"));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::copy(): runtime was destroyed previously", e.error_message());
        }
    }

    // move() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->move(root, "file2"));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::move(): runtime was destroyed previously", e.error_message());
        }
    }

    // parents() on root with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->parents());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::parents(): runtime was destroyed previously", e.error_message());
        }
    }

    // parents() on file with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->parents());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::parents(): runtime was destroyed previously", e.error_message());
        }
    }

    // parent_ids() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->parent_ids();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::parent_ids(): runtime was destroyed previously", e.error_message());
        }
    }

    // parent_ids() on root with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            root->parent_ids();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::parent_ids(): runtime was destroyed previously", e.error_message());
        }
    }

    // delete_item() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->delete_item());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::delete_item(): runtime was destroyed previously", e.error_message());
        }
    }

    // delete_item() on root with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->delete_item());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::delete_item(): runtime was destroyed previously", e.error_message());
        }
    }

    // creation_time() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->creation_time();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::creation_time(): runtime was destroyed previously", e.error_message());
        }
    }

    // native_metadata() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->native_metadata();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::native_metadata(): runtime was destroyed previously", e.error_message());
        }
    }

    // name() on root with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            root->name();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::name(): runtime was destroyed previously", e.error_message());
        }
    }

    // name() on folder with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto folder = call(root->create_folder("folder"));
        runtime->shutdown();
        try
        {
            folder->name();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::name(): runtime was destroyed previously", e.error_message());
        }
    }

    // name() on file with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->name();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("File::name(): runtime was destroyed previously", e.error_message());
        }
    }

    // list() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->list());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Folder::list(): runtime was destroyed previously", e.error_message());
        }
    }

    // lookup() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->lookup("file"));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Folder::lookup(): runtime was destroyed previously", e.error_message());
        }
    }

    // create_folder() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->create_folder("folder"));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Folder::create_folder(): runtime was destroyed previously", e.error_message());
        }
    }

    // create_file() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->create_file("file", 0));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Folder::create_file(): runtime was destroyed previously", e.error_message());
        }
    }

    // size() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            file->size();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("File::size(): runtime was destroyed previously", e.error_message());
        }
    }

    // create_uploader() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->create_uploader(ConflictPolicy::overwrite, 0));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("File::create_uploader(): runtime was destroyed previously", e.error_message());
        }
    }

    // create_downloader() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = write_file(root, "file", "");
        runtime->shutdown();
        try
        {
            call(file->create_downloader());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("File::create_downloader(): runtime was destroyed previously", e.error_message());
        }
    }

    // free_space_bytes() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->free_space_bytes());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Root::free_space_bytes(): runtime was destroyed previously", e.error_message());
        }
    }

    // used_space_bytes() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->used_space_bytes());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Root::used_space_bytes(): runtime was destroyed previously", e.error_message());
        }
    }

    // get() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create();
        auto root = get_root(runtime);
        clear_folder(root);

        runtime->shutdown();
        try
        {
            call(root->get("some_id"));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Root::get(): runtime was destroyed previously", e.error_message());
        }
    }
}

int main(int argc, char** argv)
{
    boost::system::error_code ec;
    boost::filesystem::remove_all(TEST_DIR "/storage-framework", ec);
    setenv("STORAGE_FRAMEWORK_ROOT", TEST_DIR, true);

    QCoreApplication app(argc, argv);
    qRegisterMetaType<QLocalSocket::LocalSocketState>();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
