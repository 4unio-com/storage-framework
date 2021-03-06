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

#include <unity/storage/qt/Downloader.h>

#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>

namespace unity
{
namespace storage
{
namespace qt
{
namespace internal
{

class DownloaderImpl : public QObject
{
    Q_OBJECT
public:
    DownloaderImpl(std::shared_ptr<ItemImpl> const& item_impl,
                   QString const& method,
                   QDBusPendingReply<QString, QDBusUnixFileDescriptor>& reply);
    DownloaderImpl(StorageError const& e);
    virtual ~DownloaderImpl();

    bool isValid() const;
    Downloader::Status status() const;
    StorageError error() const;
    Item item() const;

    void cancel();

    // From QLocalSocket interface.
    void close();
    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool canReadLine() const;
    bool isSequential() const;
    bool waitForBytesWritten(int msecs);
    bool waitForReadyRead(int msecs);
    qint64 readData(char* data, qint64 c);
    qint64 writeData(char const* data, qint64 c);

    static Downloader* make_job(std::shared_ptr<ItemImpl> const& item_impl,
                                QString const& method,
                                QDBusPendingReply<QString, QDBusUnixFileDescriptor>& reply);
    static Downloader* make_job(StorageError const& e);

private:
    Downloader* public_instance_;
    Downloader::Status status_;
    StorageError error_;
    std::shared_ptr<ItemImpl> item_impl_;
    QString download_id_;
    QDBusUnixFileDescriptor fd_;
    QLocalSocket socket_;
    bool finalizing_ = false;
};

}  // namespace internal
}  // namespace qt
}  // namespace storage
}  // namespace unity
