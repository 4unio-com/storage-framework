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

#include <unity/storage/visibility.h>

#include <string>

namespace unity
{
namespace storage
{
namespace provider
{

// Note: Adding new exception types also requires updating the marshaling and
//       unmarshaling code for exceptions in the client and server APIs.

/**
\brief Base exception class for all server-side exceptions.
*/
class UNITY_STORAGE_EXPORT StorageException : public std::exception
{
public:
    StorageException(std::string const& exception_type, std::string const& error_message);
    ~StorageException();

    virtual char const* what() const noexcept override;

    std::string type() const;
    std::string error_message() const;

private:
    std::string what_string_;
    std::string type_;
    std::string error_message_;
};

/**
\brief Indicates errors in the communication between the storage provider and the cloud service.
*/
class UNITY_STORAGE_EXPORT RemoteCommsException : public StorageException
{
public:
    RemoteCommsException(std::string const& error_message);
    ~RemoteCommsException();
};

/**
\brief Indicates that an item does not exist or could not be found.
*/
class UNITY_STORAGE_EXPORT NotExistsException : public StorageException
{
public:
    NotExistsException(std::string const& error_message, std::string const& key);
    ~NotExistsException();

    std::string key() const;

private:
    std::string key_;
};

/**
\brief Indicates that an item cannot be created because it exists already.
*/
class UNITY_STORAGE_EXPORT ExistsException : public StorageException
{
public:
    ExistsException(std::string const& error_message, std::string const& identity, std::string const& name);
    ~ExistsException();

    std::string native_identity() const;
    std::string name() const;

private:
    std::string identity_;
    std::string name_;
};

/**
\brief Indicates that an upload detected a version mismatch.
*/
class UNITY_STORAGE_EXPORT ConflictException : public StorageException
{
public:
    ConflictException(std::string const& error_message);
    ~ConflictException();
};

/**
\brief Indicates that an operation failed because of a permission problem.
*/
class UNITY_STORAGE_EXPORT PermissionException : public StorageException
{
public:
    PermissionException(std::string const& error_message);
    ~PermissionException();
};

/**
\brief Indicates that an update failed because the provider ran out of space.
*/
class UNITY_STORAGE_EXPORT QuotaException : public StorageException
{
public:
    QuotaException(std::string const& error_message);
    ~QuotaException();
};

/**
\brief Indicates that an upload or download was cancelled before it could complete.
*/
class UNITY_STORAGE_EXPORT CancelledException : public StorageException
{
public:
    CancelledException(std::string const& error_message);
    ~CancelledException();
};

/**
\brief Indicates incorrect use of the API, such as calling methods in the wrong order.
*/
class UNITY_STORAGE_EXPORT LogicException : public StorageException
{
public:
    LogicException(std::string const& error_message);
    ~LogicException();
};

/**
\brief Indicates an invalid parameter, such as a negative value when a positive one was
expected, or a string that does not parse correctly or is empty when it should be non-empty.
*/
class UNITY_STORAGE_EXPORT InvalidArgumentException : public StorageException
{
public:
    InvalidArgumentException(std::string const& error_message);
    ~InvalidArgumentException();
};

/**
\brief Indicates a system error, such as failure to create a file or folder,
or any other (usually non-recoverable) kind of error that should not arise during normal operation.
*/
class UNITY_STORAGE_EXPORT ResourceException : public StorageException
{
public:
    ResourceException(std::string const& error_message, int error_code);
    ~ResourceException();

    int error_code() const noexcept;

private:
    int error_code_;
};

/**
\brief Indicates that the server side caught an exception that does not derive from
StorageException, such as a std::exception, or caught some other unknown type (such as `int`).
*/
class UNITY_STORAGE_EXPORT UnknownException : public StorageException
{
public:
    UnknownException(std::string const& error_message);
    ~UnknownException();
};

}  // namespace provider
}  // namespace storage
}  // namespace unity
