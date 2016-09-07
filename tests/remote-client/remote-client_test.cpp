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

#include <utils/DBusEnvironment.h>
#include <utils/ProviderFixture.h>
#include <MockProvider.h>

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

using namespace unity::storage;
using namespace unity::storage::qt::client;
using namespace std;

// Yes, that's ridiculously long, but the builders in Jenkins and the CI Train
// are stupifyingly slow at times.
static constexpr int SIGNAL_WAIT_TIME = 30000;

class RemoteClientTest : public ::testing::Test
{
public:
    QDBusConnection const& connection()
    {
        return dbus_->connection();
    }

protected:
    void SetUp() override
    {
        dbus_.reset(new DBusEnvironment);
        dbus_->add_demo_provider("google-drive-scope");
        dbus_->start_services();
    }

    void TearDown() override
    {
        dbus_.reset();
    }

private:
    unique_ptr<DBusEnvironment> dbus_;
};

class RuntimeTest : public RemoteClientTest {};
class RootTest : public RemoteClientTest {};
class FolderTest : public RemoteClientTest {};
class FileTest : public RemoteClientTest {};
class ItemTest : public RemoteClientTest {};

class DestroyedTest : public ProviderFixture
{
protected:
    void SetUp() override
    {
        runtime_ = Runtime::create(connection());
        acc_ = runtime_->make_test_account(bus_name(), object_path());
    }

    void TearDown() override
    {
    }

    Runtime::SPtr runtime_;
    Account::SPtr acc_;
};

// Bunch of helper functions to reduce the amount of noise in the tests.

template<typename T>
bool wait(T fut)
{
    QFutureWatcher<decltype(fut.result())> w;
    QSignalSpy spy(&w, &decltype(w)::finished);
    w.setFuture(fut);
    bool rc = spy.wait(SIGNAL_WAIT_TIME);
    EXPECT_TRUE(rc);
    return rc;
}

template<>
bool wait(QFuture<void> fut)
{
    QFutureWatcher<void> w;
    QSignalSpy spy(&w, &decltype(w)::finished);
    w.setFuture(fut);
    bool rc = spy.wait(SIGNAL_WAIT_TIME);
    EXPECT_TRUE(rc);
    return rc;
}

template <typename T>
T call(QFuture<T> fut)
{
    if (!wait(fut))
    {
        throw runtime_error("call timed out");
    }
    return fut.result();
}

template <>
void call(QFuture<void> fut)
{
    if (!wait(fut))
    {
        throw runtime_error("call timed out");
    }
    fut.waitForFinished();
}

Account::SPtr get_account(Runtime::SPtr const& runtime)
{
    auto accounts = call(runtime->accounts());
    if (accounts.size() == 0)
    {
        qCritical() << "Cannot find any online account";
        qCritical() << "Configure at least one online account for a provider in System Settings -> Online Accounts";
        return nullptr;
    }
    for (auto acc : accounts)
    {
        if (acc->owner_id() == "google-drive-scope")
        {
            return acc;
        }
    }
    abort();  // Impossible
}

Root::SPtr get_root(Runtime::SPtr const& runtime)
{
    auto acc = get_account(runtime);
    auto roots = call(acc->roots());
    assert(roots.size() == 1);
    return roots[0];
}

Folder::SPtr get_parent(Item::SPtr const& item)
{
    assert(item->type() != ItemType::root);
    auto parents = call(item->parents());
    assert(parents.size() >= 1);
    return parents[0];
}

void clear_folder(Folder::SPtr const& folder)
{
    auto items = call(folder->list());
    assert(items.size() != 0);  // TODO: temporary hack for use with demo provider
    for (auto i : items)
    {
        call(i->delete_item());
    }
}

TEST_F(RuntimeTest, lifecycle)
{
    auto runtime = Runtime::create(connection());
    runtime->shutdown();
    runtime->shutdown();  // Just to show that this is safe.
}

TEST_F(RuntimeTest, basic)
{
    auto runtime = Runtime::create(connection());

    auto acc = get_account(runtime);
    EXPECT_EQ(runtime, acc->runtime());
    EXPECT_EQ("", acc->owner());
    EXPECT_EQ("google-drive-scope", acc->owner_id());
    EXPECT_EQ("Fake google account", acc->description());
}

TEST_F(RuntimeTest, roots)
{
    auto runtime = Runtime::create(connection());

    auto acc = get_account(runtime);
    ASSERT_NE(nullptr, acc);
    auto roots = call(acc->roots());
    ASSERT_GE(roots.size(), 0);
    EXPECT_EQ("root_id", roots[0]->native_identity());
}

TEST_F(RootTest, basic)
{
    auto runtime = Runtime::create(connection());

    auto acc = get_account(runtime);
    auto root = get_root(runtime);
    EXPECT_EQ("root_id", root->native_identity());
    EXPECT_EQ(acc, root->account());
    EXPECT_EQ(ItemType::root, root->type());
    EXPECT_EQ("Root", root->name());
    EXPECT_NE("", root->etag());

    auto parents = call(root->parents());
    EXPECT_TRUE(parents.isEmpty());
    EXPECT_TRUE(root->parent_ids().isEmpty());

    // get(<root-ID>) must return the root.
    auto item = call(root->get(root->native_identity()));
    EXPECT_NE(nullptr, dynamic_pointer_cast<Root>(item));
    EXPECT_TRUE(root->equal_to(item));

    // Free and used space can be anything, but must be > 0.
    auto free_space = call(root->free_space_bytes());
    cerr << "bytes free: " << free_space << endl;
    EXPECT_GT(free_space, 0);

    auto used_space = call(root->used_space_bytes());
    cerr << "bytes used: " << used_space << endl;
    EXPECT_GT(used_space, 0);
}

TEST_F(FolderTest, basic)
{
    auto runtime = Runtime::create(connection());

    auto root = get_root(runtime);
    clear_folder(root);

    auto items = call(root->list());
    ASSERT_EQ(1, items.size());

    // Create a file and check that it was created with correct type, name, and size 0.
    auto uploader = call(root->create_file("file1", 10));
    EXPECT_EQ(10, uploader->size());
    auto file = call(uploader->finish_upload());
    EXPECT_EQ(ItemType::file, file->type());
    EXPECT_EQ("some_upload", file->name());
    EXPECT_EQ(10, file->size());
    EXPECT_EQ("some_id", file->native_identity());

    // For coverage: getting a file must return the correct one.
    file = dynamic_pointer_cast<File>(call(root->get("child_id")));
    EXPECT_EQ("child_id", file->native_identity());
    EXPECT_EQ("Child", file->name());
}

TEST_F(FileTest, upload)
{
    auto runtime = Runtime::create(connection());

    auto root = get_root(runtime);
    clear_folder(root);

    // Get a file.
    auto children = call(root->lookup("Child"));
    ASSERT_EQ(1, children.size());
    auto file = dynamic_pointer_cast<File>(children[0]);
    EXPECT_EQ("child_id", file->native_identity());
    EXPECT_EQ("Child", file->name());

    auto uploader = call(file->create_uploader(ConflictPolicy::error_if_conflict, 0));
    EXPECT_EQ(0, uploader->size());

    auto uploaded_file = call(uploader->finish_upload());
    EXPECT_EQ("some_id", uploaded_file->native_identity());
    EXPECT_EQ("some_upload", uploaded_file->name());
}

TEST_F(RootTest, root_exceptions)
{
    auto runtime = Runtime::create(connection());

    auto root = get_root(runtime);
    clear_folder(root);

    try
    {
        call(root->delete_item());
        FAIL();
    }
    catch (LogicException const& e)
    {
        EXPECT_EQ("Item::delete_item(): cannot delete root folder", e.error_message()) << e.what();
    }

    {
        try
        {
            call(root->get("no_such_file_id"));
            FAIL();
        }
        catch (NotExistsException const& e)
        {
            EXPECT_EQ("no_such_file_id", e.key());
        }
    }
}

TEST_F(RuntimeTest, runtime_destroyed_exceptions)
{
    // Getting the runtime from an account after shutting down the runtime must fail.
    {
        auto runtime = Runtime::create(connection());
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

    // Getting the runtime from an account after destroying the runtime must fail.
    {
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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

    // Getting roots from an account after shutting down the runtime must fail.
    {
        auto runtime = Runtime::create(connection());
        auto acc = get_account(runtime);
        runtime->shutdown();
        try
        {
            call(acc->roots());
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Account::roots(): runtime was destroyed previously", e.error_message());
        }
    }

    // Getting the account from a root with a destroyed runtime must fail.
    {
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto acc = get_account(runtime);
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
        runtime.reset();
        acc.reset();
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
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
            EXPECT_EQ("Root::parents(): runtime was destroyed previously", e.error_message());
        }
    }

    // parents() on file with destroyed runtime must fail.
    {
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
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
            EXPECT_EQ("Root::parent_ids(): runtime was destroyed previously", e.error_message());
        }
    }

    // delete_item() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto folder = dynamic_pointer_cast<Folder>(call(root->get("child_folder_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
        runtime->shutdown();
        try
        {
            file->name();
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("Item::name(): runtime was destroyed previously", e.error_message());
        }
    }

    // list() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
        runtime->shutdown();
        try
        {
            call(file->create_uploader(ConflictPolicy::overwrite, 0));
            FAIL();
        }
        catch (RuntimeDestroyedException const& e)
        {
            EXPECT_EQ("File::create_uploader(): runtime was destroyed previously", e.error_message()) << e.what();
        }
    }

    // create_downloader() with destroyed runtime must fail.
    {
        auto runtime = Runtime::create(connection());
        auto root = get_root(runtime);
        clear_folder(root);

        auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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
        auto runtime = Runtime::create(connection());
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

TEST_F(DestroyedTest, roots_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider));

    auto fut = acc_->roots();
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Account::roots(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, get_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("metadata slow")));

    auto root = call(acc_->roots())[0];
    auto fut = root->get("root_id");
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Root::get(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, copy_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider()));

    auto root = call(acc_->roots())[0];
    auto fut = root->copy(root, "new name");
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Item::copy(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, move_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("move slow")));

    auto root = call(acc_->roots())[0];
    auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
    auto fut = file->move(root, "new name");
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Item::move(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, list_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("list slow")));

    auto root = call(acc_->roots())[0];
    auto fut = root->list();
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Folder::list(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, lookup_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("lookup slow")));

    auto root = call(acc_->roots())[0];
    auto fut = root->lookup("Child");
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Folder::lookup(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, create_folder_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("create_folder slow")));

    auto root = call(acc_->roots())[0];
    auto fut = root->create_folder("Child");
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Folder::create_folder(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, create_file_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("create_file slow")));

    auto root = call(acc_->roots())[0];
    auto fut = root->create_file("Child", 0);
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("Folder::create_file(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, create_uploader_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("create_file slow")));

    auto root = call(acc_->roots())[0];
    auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
    auto fut = file->create_uploader(ConflictPolicy::overwrite, 0);
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("File::create_uploader(): runtime was destroyed previously", e.error_message());
    }
}

TEST_F(DestroyedTest, create_downloader_destroyed_while_reply_outstanding)
{
    set_provider(unique_ptr<provider::ProviderBase>(new MockProvider("create_file slow")));

    auto root = call(acc_->roots())[0];
    auto file = dynamic_pointer_cast<File>(call(root->get("child_id")));
    auto fut = file->create_downloader();
    runtime_->shutdown();
    try
    {
        ASSERT_TRUE(wait(fut));
        fut.result();
        FAIL();
    }
    catch (RuntimeDestroyedException const& e)
    {
        EXPECT_EQ("File::create_downloader(): runtime was destroyed previously", e.error_message());
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
