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

#include <unity/storage/qt/client/Exceptions.h>

using namespace std;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

StorageException::StorageException(char const* exception_name, QString const& error_message)
    : what_string_(string(exception_name) + ": " + error_message.toStdString())
    , error_message_(error_message)
{
}

StorageException::~StorageException() = default;

char const* StorageException::what() const noexcept
{
    return what_string_.c_str();
}

QString StorageException::error_message() const
{
    return error_message_;
}

LocalCommsException::LocalCommsException(QString const& error_message)
    : StorageException("LocalCommsException", error_message)
{
}

LocalCommsException::~LocalCommsException() = default;

LocalCommsException* LocalCommsException::clone() const
{
    return new LocalCommsException(*this);
}

void LocalCommsException::raise() const
{
    throw *this;
}

RemoteCommsException::RemoteCommsException(QString const& error_message)
    : StorageException("RemoteCommsException", error_message)
{
}

RemoteCommsException::~RemoteCommsException() = default;

RemoteCommsException* RemoteCommsException::clone() const
{
    return new RemoteCommsException(*this);
}

void RemoteCommsException::raise() const
{
    throw *this;
}

DeletedException::DeletedException(QString const& error_message, QString const& identity)
    : StorageException("DeletedException", error_message)
    , identity_(identity)
{
}

DeletedException::~DeletedException() = default;

DeletedException* DeletedException::clone() const
{
    return new DeletedException(*this);
}

void DeletedException::raise() const
{
    throw *this;
}

QString DeletedException::native_identity() const
{
    return identity_;
}

RuntimeDestroyedException::RuntimeDestroyedException(QString const& method)
    : StorageException("RuntimeDestroyedException", method + ": runtime was destroyed previously")
{
}

RuntimeDestroyedException::~RuntimeDestroyedException() = default;

RuntimeDestroyedException* RuntimeDestroyedException::clone() const
{
    return new RuntimeDestroyedException(*this);
}

void RuntimeDestroyedException::raise() const
{
    throw *this;
}

NotExistsException::NotExistsException(QString const& error_message, QString const& key)
    : StorageException("NotExistsException", error_message)
    , key_(key)
{
}

NotExistsException::~NotExistsException() = default;

NotExistsException* NotExistsException::clone() const
{
    return new NotExistsException(*this);
}

void NotExistsException::raise() const
{
    throw *this;
}

QString NotExistsException::key() const
{
    return key_;
}

ExistsException::ExistsException(QString const& error_message, QString const& identity, QString const& name)
    : StorageException("ExistsException", error_message)
    , identity_(identity)
    , name_(name)
{
}

ExistsException::~ExistsException() = default;

ExistsException* ExistsException::clone() const
{
    return new ExistsException(*this);
}

void ExistsException::raise() const
{
    throw *this;
}

QString ExistsException::native_identity() const
{
    return identity_;
}

QString ExistsException::name() const
{
    return name_;
}

ConflictException::ConflictException(QString const& error_message)
    : StorageException("ConflictException", error_message)
{
}

ConflictException::~ConflictException() = default;

ConflictException* ConflictException::clone() const
{
    return new ConflictException(*this);
}

void ConflictException::raise() const
{
    throw *this;
}

PermissionException::PermissionException(QString const& error_message)
    : StorageException("PermissionException", error_message)
{
}

PermissionException::~PermissionException() = default;

PermissionException* PermissionException::clone() const
{
    return new PermissionException(*this);
}

void PermissionException::raise() const
{
    throw *this;
}

QuotaException::QuotaException(QString const& error_message)
    : StorageException("QuotaException", error_message)
{
}

QuotaException::~QuotaException() = default;

QuotaException* QuotaException::clone() const
{
    return new QuotaException(*this);
}

void QuotaException::raise() const
{
    throw *this;
}

CancelledException::CancelledException(QString const& error_message)
    : StorageException("CancelledException", error_message)
{
}

CancelledException::~CancelledException() = default;

CancelledException* CancelledException::clone() const
{
    return new CancelledException(*this);
}

void CancelledException::raise() const
{
    throw *this;
}

LogicException::LogicException(QString const& error_message)
    : StorageException("LogicException", error_message)
{
}

LogicException::~LogicException() = default;

LogicException* LogicException::clone() const
{
    return new LogicException(*this);
}

void LogicException::raise() const
{
    throw *this;
}

InvalidArgumentException::InvalidArgumentException(QString const& error_message)
    : StorageException("InvalidArgumentException", error_message)
{
}

InvalidArgumentException::~InvalidArgumentException() = default;

InvalidArgumentException* InvalidArgumentException::clone() const
{
    return new InvalidArgumentException(*this);
}

void InvalidArgumentException::raise() const
{
    throw *this;
}

ResourceException::ResourceException(QString const& error_message, int error_code)
    : StorageException("ResourceException", error_message)
    , error_code_(error_code)
{
}

ResourceException::~ResourceException() = default;

ResourceException* ResourceException::clone() const
{
    return new ResourceException(*this);
}

void ResourceException::raise() const
{
    throw *this;
}

int ResourceException::error_code() const noexcept
{
    return error_code_;
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
