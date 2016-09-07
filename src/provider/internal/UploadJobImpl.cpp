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

#include <unity/storage/provider/internal/UploadJobImpl.h>
#include <unity/storage/internal/safe_strerror.h>
#include <unity/storage/provider/Exceptions.h>
#include <unity/storage/provider/UploadJob.h>
#include <unity/storage/provider/internal/MainLoopExecutor.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <stdexcept>

using namespace std;
using namespace unity::storage::internal;

namespace unity
{
namespace storage
{
namespace provider
{
namespace internal
{

UploadJobImpl::UploadJobImpl(std::string const& upload_id)
    : upload_id_(upload_id)
{
    int socks[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0)
    {
        int error_code = errno;
        string msg = "could not create socketpair: " + safe_strerror(error_code);
        throw ResourceException(msg, error_code);
    }
    read_socket_ = socks[0];
    write_socket_ = socks[1];

#if 0
    // TODO: We should be able to half-close the write channel of the read socket, and the read channel of
    // the write socket. But, if we do, QLocalSocket indicates that everything was closed, which causes
    // failures on the client side. We suspect a QLocalSocket bug -- need to investigate.
    if (shutdown(read_socket_, SHUT_WR) < 0)
    {
        int error_code = errno;
        string msg = "could not shut down write channel on read socket: " + safe_strerror(error_code);
        throw ResourceException(msg, error_code);
    }
    if (shutdown(write_socket_, SHUT_RD) < 0)
    {
        int error_code = errno;
        string msg = "Could not shut down read channel on write socket" + safe_strerror(error_code);
        throw ResourceException(msg, error_code);
    }
#endif
}

UploadJobImpl::~UploadJobImpl()
{
    if (read_socket_ >= 0)
    {
        close(read_socket_);
    }
    if (write_socket_ >= 0)
    {
        close(write_socket_);
    }
}

void UploadJobImpl::complete_init()
{
}

string const& UploadJobImpl::upload_id() const
{
    return upload_id_;
}

int UploadJobImpl::read_socket() const
{
    return read_socket_;
}

int UploadJobImpl::take_write_socket()
{
    assert(write_socket_ >= 0);
    int sock = write_socket_;
    write_socket_ = -1;
    return sock;
}

void UploadJobImpl::report_error(exception_ptr p)
{
    if (read_socket_ >= 0)
    {
        close(read_socket_);
        read_socket_ = -1;
    }

    lock_guard<mutex> guard(completion_lock_);
    completed_ = true;
    // Convert std::exception_ptr to boost::exception_ptr
    try
    {
        rethrow_exception(p);
    }
    catch (StorageException const& e)
    {
        completion_promise_.set_exception(e);
    }
    catch (...)
    {
        completion_promise_.set_exception(boost::current_exception());
    }
}

boost::future<Item> UploadJobImpl::finish(UploadJob& job)
{
    lock_guard<mutex> guard(completion_lock_);
    if (completed_)
    {
        return completion_promise_.get_future();
    }
    return job.finish();
}

boost::future<void> UploadJobImpl::cancel(UploadJob& job)
{
    lock_guard<mutex> guard(completion_lock_);
    if (completed_)
    {
        return boost::make_ready_future();
    }
    return job.cancel();
}

}
}
}
}
