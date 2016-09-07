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

#include <unity/storage/provider/Exceptions.h>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{

StorageException::StorageException(std::string const& exception_type, string const& error_message)
    : what_string_(string(exception_type) + ": " + error_message)
    , type_(exception_type)
    , error_message_(error_message)
{
}

StorageException::~StorageException() = default;

char const* StorageException::what() const noexcept
{
    return what_string_.c_str();
}

string StorageException::type() const
{
    return type_;
}

string StorageException::error_message() const
{
    return error_message_;
}

RemoteCommsException::RemoteCommsException(string const& error_message)
    : StorageException("RemoteCommsException", error_message)
{
}

RemoteCommsException::~RemoteCommsException() = default;

NotExistsException::NotExistsException(string const& error_message, string const& key)
    : StorageException("NotExistsException", error_message)
    , key_(key)
{
}

NotExistsException::~NotExistsException() = default;

string NotExistsException::key() const
{
    return key_;
}

ExistsException::ExistsException(string const& error_message, string const& identity, string const& name)
    : StorageException("ExistsException", error_message)
    , identity_(identity)
    , name_(name)
{
}

ExistsException::~ExistsException() = default;

string ExistsException::native_identity() const
{
    return identity_;
}

string ExistsException::name() const
{
    return name_;
}

ConflictException::ConflictException(string const& error_message)
    : StorageException("ConflictException", error_message)
{
}

ConflictException::~ConflictException() = default;

PermissionException::PermissionException(string const& error_message)
    : StorageException("PermissionException", error_message)
{
}

PermissionException::~PermissionException() = default;

QuotaException::QuotaException(string const& error_message)
    : StorageException("QuotaException", error_message)
{
}

QuotaException::~QuotaException() = default;

CancelledException::CancelledException(string const& error_message)
    : StorageException("CancelledException", error_message)
{
}

CancelledException::~CancelledException() = default;

LogicException::LogicException(string const& error_message)
    : StorageException("LogicException", error_message)
{
}

LogicException::~LogicException() = default;

InvalidArgumentException::InvalidArgumentException(string const& error_message)
    : StorageException("InvalidArgumentException", error_message)
{
}

InvalidArgumentException::~InvalidArgumentException() = default;

ResourceException::ResourceException(string const& error_message, int error_code)
    : StorageException("ResourceException", error_message)
    , error_code_(error_code)
{
}

ResourceException::~ResourceException() = default;

int ResourceException::error_code() const noexcept
{
    return error_code_;
}

UnknownException::UnknownException(string const& error_message)
    : StorageException("UnknownException", error_message)
{
}

UnknownException::~UnknownException() = default;

}  // namespace provider
}  // namespace storage
}  // namespace unity
