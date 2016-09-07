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

#include <unity/storage/provider/internal/ProviderInterface.h>
#include <unity/storage/provider/DownloadJob.h>
#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/ProviderBase.h>
#include <unity/storage/provider/UploadJob.h>
#include <unity/storage/provider/internal/AccountData.h>
#include <unity/storage/provider/internal/DownloadJobImpl.h>
#include <unity/storage/provider/internal/MainLoopExecutor.h>
#include <unity/storage/provider/internal/PendingJobs.h>
#include <unity/storage/provider/internal/UploadJobImpl.h>
#include <unity/storage/provider/internal/dbusmarshal.h>

#include <OnlineAccounts/AuthenticationData>
#include <QDebug>

using namespace std;

namespace unity {
namespace storage {
namespace provider {
namespace internal {

ProviderInterface::ProviderInterface(shared_ptr<AccountData> const& account, QObject *parent)
    : QObject(parent), account_(account)
{
}

ProviderInterface::~ProviderInterface() = default;

void ProviderInterface::queue_request(Handler::Callback callback)
{
    unique_ptr<Handler> handler(
        new Handler(account_, callback, connection(), message()));
    connect(handler.get(), &Handler::finished, this, &ProviderInterface::request_finished);
    setDelayedReply(true);
    // If we haven't retrieved the authentication details from
    // OnlineAccounts, delay processing the handler until then.
    if (account_->has_credentials())
    {
        handler->begin();
    }
    else
    {
        account_->authenticate(true);
        connect(account_.get(), &AccountData::authenticated,
                handler.get(), &Handler::begin);
    }
    requests_.emplace(handler.get(), std::move(handler));
}

void ProviderInterface::request_finished()
{
    Handler* handler = static_cast<Handler*>(sender());
    try
    {
        auto& h = requests_.at(handler);
        h.release();
        requests_.erase(handler);
    }
    // LCOV_EXCL_START
    catch (std::out_of_range const& e)
    {
        qWarning() << "finished() called on unknown handler" << handler;
    }
    // LCOV_EXCL_STOP

    // Queue deletion of handler once we re-enter the event loop.
    handler->deleteLater();
}

QList<ProviderInterface::IMD> ProviderInterface::Roots()
{
    queue_request([](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().roots(ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto roots = f.get();
                    return message.createReply(QVariant::fromValue(roots));
                });
        });
    return {};
}

QList<ProviderInterface::IMD> ProviderInterface::List(QString const& item_id, QString const& page_token, QString& /*next_token*/)
{
    queue_request([item_id, page_token](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().list(item_id.toStdString(), page_token.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    vector<Item> children;
                    string next_token;
                    tie(children, next_token) = f.get();
                    return message.createReply({
                            QVariant::fromValue(children),
                            QVariant(QString::fromStdString(next_token)),
                        });
                });
        });
    return {};
}

QList<ProviderInterface::IMD> ProviderInterface::Lookup(QString const& parent_id, QString const& name)
{
    queue_request([parent_id, name](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().lookup(parent_id.toStdString(), name.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto items = f.get();
                    return message.createReply(QVariant::fromValue(items));
                });
        });
    return {};
}

ProviderInterface::IMD ProviderInterface::Metadata(QString const& item_id)
{
    queue_request([item_id](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().metadata(item_id.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto item = f.get();
                    return message.createReply(QVariant::fromValue(item));
                });
        });
    return {};
}

ProviderInterface::IMD ProviderInterface::CreateFolder(QString const& parent_id, QString const& name)
{
    queue_request([parent_id, name](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().create_folder(
                parent_id.toStdString(), name.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto item = f.get();
                    return message.createReply(QVariant::fromValue(item));
                });
        });
    return {};
}

QString ProviderInterface::CreateFile(QString const& parent_id, QString const& name, int64_t size, QString const& content_type, bool allow_overwrite, QDBusUnixFileDescriptor& /*file_descriptor*/)
{
    queue_request([parent_id, name, size, content_type, allow_overwrite](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().create_file(
                parent_id.toStdString(), name.toStdString(),
                size, content_type.toStdString(), allow_overwrite, ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto job = f.get();
                    auto upload_id = QString::fromStdString(job->upload_id());
                    QDBusUnixFileDescriptor file_desc;
                    int fd = job->p_->take_write_socket();
                    file_desc.setFileDescriptor(fd);
                    close(fd);

                    account->jobs().add_upload(message.service(), std::move(job));
                    return message.createReply({
                            QVariant(upload_id),
                            QVariant::fromValue(file_desc),
                        });
                });
        });
    return "";
}

QString ProviderInterface::Update(QString const& item_id, int64_t size, QString const& old_etag, QDBusUnixFileDescriptor& /*file_descriptor*/)
{
    queue_request([item_id, size, old_etag](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().update(
                item_id.toStdString(), size, old_etag.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto job = f.get();
                    auto upload_id = QString::fromStdString(job->upload_id());
                    QDBusUnixFileDescriptor file_desc;
                    int fd = job->p_->take_write_socket();
                    file_desc.setFileDescriptor(fd);
                    close(fd);

                    account->jobs().add_upload(message.service(), std::move(job));
                    return message.createReply({
                            QVariant(upload_id),
                            QVariant::fromValue(file_desc),
                        });
                });
        });
    return "";
}

ProviderInterface::IMD ProviderInterface::FinishUpload(QString const& upload_id)
{
    queue_request([upload_id](shared_ptr<AccountData> const& account, Context const& /*ctx*/, QDBusMessage const& message) {
            // FIXME: removing the job at this point means we can't
            // cancel during finish().
            // Throws if job is not available
            auto job = account->jobs().remove_upload(message.service(), upload_id.toStdString());
            auto f = job->p_->finish(*job);
            return f.then(
                EXEC_IN_MAIN
                [account, message, job](decltype(f) f) -> QDBusMessage {
                    auto item = f.get();
                    return message.createReply(QVariant::fromValue(item));
                });
        });
    return {};
}

void ProviderInterface::CancelUpload(QString const& upload_id)
{
    queue_request([upload_id](shared_ptr<AccountData> const& account, Context const& /*ctx*/, QDBusMessage const& message) {
            // Throws if job is not available
            auto job = account->jobs().remove_upload(message.service(), upload_id.toStdString());
            auto f = job->p_->cancel(*job);
            return f.then(
                EXEC_IN_MAIN
                [account, message, job](decltype(f) f) -> QDBusMessage {
                    f.get();
                    return message.createReply();
                });
        });
}

QString ProviderInterface::Download(QString const& item_id, QDBusUnixFileDescriptor& /*file_descriptor*/)
{
    queue_request([item_id](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().download(
                item_id.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto job = f.get();
                    auto download_id = QString::fromStdString(job->download_id());
                    QDBusUnixFileDescriptor file_desc;
                    int fd = job->p_->take_read_socket();
                    file_desc.setFileDescriptor(fd);
                    close(fd);

                    account->jobs().add_download(message.service(), std::move(job));
                    return message.createReply({
                            QVariant(download_id),
                            QVariant::fromValue(file_desc),
                        });
                });
        });
    return "";
}

void ProviderInterface::FinishDownload(QString const& download_id)
{
    queue_request([download_id](shared_ptr<AccountData> const& account, Context const& /*ctx*/, QDBusMessage const& message) {
            // FIXME: removing the job at this point means we can't
            // cancel during finish().
            // Throws if job is not available
            auto job = account->jobs().remove_download(message.service(), download_id.toStdString());
            auto f = job->p_->finish(*job);
            return f.then(
                EXEC_IN_MAIN
                [account, message, job](decltype(f) f) -> QDBusMessage {
                    f.get();
                    return message.createReply();
                });
        });
}

void ProviderInterface::Delete(QString const& item_id)
{
    queue_request([item_id](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().delete_item(
                item_id.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    f.get();
                    return message.createReply();
                });
        });
}

ProviderInterface::IMD ProviderInterface::Move(QString const& item_id, QString const& new_parent_id, QString const& new_name)
{
    queue_request([item_id, new_parent_id, new_name](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().move(
                item_id.toStdString(), new_parent_id.toStdString(),
                new_name.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto item = f.get();
                    return message.createReply(QVariant::fromValue(item));
                });
        });
    return {};
}

ProviderInterface::IMD ProviderInterface::Copy(QString const& item_id, QString const& new_parent_id, QString const& new_name)
{
    queue_request([item_id, new_parent_id, new_name](shared_ptr<AccountData> const& account, Context const& ctx, QDBusMessage const& message) {
            auto f = account->provider().copy(
                item_id.toStdString(), new_parent_id.toStdString(),
                new_name.toStdString(), ctx);
            return f.then(
                EXEC_IN_MAIN
                [account, message](decltype(f) f) -> QDBusMessage {
                    auto item = f.get();
                    return message.createReply(QVariant::fromValue(item));
                });
        });
    return {};
}

}
}
}
}
