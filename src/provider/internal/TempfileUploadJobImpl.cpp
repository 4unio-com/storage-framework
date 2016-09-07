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

#include <unity/storage/provider/internal/TempfileUploadJobImpl.h>
#include <unity/storage/provider/Exceptions.h>

#include <cassert>
#include <exception>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

TempfileUploadJobImpl::TempfileUploadJobImpl(std::string const& upload_id)
    : UploadJobImpl(upload_id)
{
}

TempfileUploadJobImpl::~TempfileUploadJobImpl() = default;

void TempfileUploadJobImpl::complete_init()
{
    tmpfile_.reset(new QTemporaryFile());
    reader_.reset(new QLocalSocket());

    assert(tmpfile_->open());

    reader_->setSocketDescriptor(
        read_socket_, QLocalSocket::ConnectedState, QIODevice::ReadOnly);
    read_socket_ = -1;
    connect(reader_.get(), &QIODevice::readyRead,
            this, &TempfileUploadJobImpl::on_ready_read);
    connect(reader_.get(), &QIODevice::readChannelFinished,
            this, &TempfileUploadJobImpl::on_read_channel_finished);
}

std::string TempfileUploadJobImpl::file_name() const
{
    if (!tmpfile_)
    {
        return "";
    }
    return tmpfile_->fileName().toStdString();
}

void TempfileUploadJobImpl::drain()
{
    while (true)
    {
        if (!tmpfile_->isOpen())
        {
            break;
        }
        if (!reader_->waitForReadyRead(0))
        {
            // Nothing was available to read: is the read channel still open?
            if (tmpfile_->isOpen())
            {
                throw LogicException("Socket not closed");
            }
        }
    }
}

void TempfileUploadJobImpl::on_ready_read()
{
    char buffer[4096];
    while (reader_->bytesAvailable() > 0)
    {
        qint64 n_read = reader_->read(buffer, sizeof(buffer));
        if (n_read > 0)
        {
            qint64 n_written = tmpfile_->write(buffer, n_read);
            assert(n_written == n_read);
        }
    }
}

void TempfileUploadJobImpl::on_read_channel_finished()
{
    // drain the socket and close the tempfile.
    on_ready_read();
    tmpfile_->close();
}

}
}
}
}
