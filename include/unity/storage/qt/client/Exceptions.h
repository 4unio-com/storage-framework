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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#include <QException>
#pragma GCC diagnostic pop
#include <QString>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

/**
\brief Base exception class for all exceptions returned by the API.
*/
class UNITY_STORAGE_EXPORT StorageException : public QException
{
public:
    StorageException(char const* exception_name, QString const& error_message);
    ~StorageException();

    virtual StorageException* clone() const override = 0;
    virtual void raise() const override = 0;

    virtual char const* what() const noexcept override;

    QString error_message() const;

private:
    std::string what_string_;
    QString error_message_;
};

/**
\brief Indicates errors in the communication with the storage provider service.
*/
class UNITY_STORAGE_EXPORT LocalCommsException : public StorageException
{
public:
    LocalCommsException(QString const& error_message);
    ~LocalCommsException();

    virtual LocalCommsException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates errors in the communication between the storage provider and the cloud service.
*/
class UNITY_STORAGE_EXPORT RemoteCommsException : public StorageException
{
public:
    RemoteCommsException(QString const& error_message);
    ~RemoteCommsException();

    virtual RemoteCommsException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates that the caller invoked an operation on a file or folder that was deleted.
*/
class UNITY_STORAGE_EXPORT DeletedException : public StorageException
{
public:
    DeletedException(QString const& error_message, QString const& identity_);
    ~DeletedException();

    virtual DeletedException* clone() const override;
    virtual void raise() const override;

    QString native_identity() const;

private:
    QString identity_;
};

/**
\brief Indicates that the caller destroyed the runtime.
*/
class UNITY_STORAGE_EXPORT RuntimeDestroyedException : public StorageException
{
public:
    RuntimeDestroyedException(QString const& method);
    ~RuntimeDestroyedException();

    virtual RuntimeDestroyedException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates that an item does not exist or could not be found.
*/
class UNITY_STORAGE_EXPORT NotExistsException : public StorageException
{
public:
    NotExistsException(QString const& error_message, QString const& key);
    ~NotExistsException();

    virtual NotExistsException* clone() const override;
    virtual void raise() const override;

    QString key() const;

private:
    QString key_;
};

/**
\brief Indicates that an item cannot be created because it exists already.
*/
class UNITY_STORAGE_EXPORT ExistsException : public StorageException
{
public:
    ExistsException(QString const& error_message, QString const& identity, QString const& name);
    ~ExistsException();

    virtual ExistsException* clone() const override;
    virtual void raise() const override;

    QString native_identity() const;
    QString name() const;

private:
    QString identity_;
    QString name_;
};

/**
\brief Indicates that an upload detected a version mismatch.
*/
class UNITY_STORAGE_EXPORT ConflictException : public StorageException
{
public:
    ConflictException(QString const& error_message);
    ~ConflictException();

    virtual ConflictException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates that an operation failed because of a permission problem.
*/
class UNITY_STORAGE_EXPORT PermissionException : public StorageException
{
public:
    PermissionException(QString const& error_message);
    ~PermissionException();

    virtual PermissionException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates that an update failed because the provider ran out of space.
*/
class UNITY_STORAGE_EXPORT QuotaException : public StorageException
{
public:
    QuotaException(QString const& error_message);
    ~QuotaException();

    virtual QuotaException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates that an upload or download was cancelled before it could complete.
*/
class UNITY_STORAGE_EXPORT CancelledException : public StorageException
{
public:
    CancelledException(QString const& error_message);
    ~CancelledException();

    virtual CancelledException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates incorrect use of the API, such as calling methods in the wrong order.
*/
class UNITY_STORAGE_EXPORT LogicException : public StorageException
{
public:
    LogicException(QString const& error_message);
    ~LogicException();

    virtual LogicException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates an invalid parameter, such as a negative value when a positive one was
expected, or a string that does not parse correctly or is empty when it should be non-empty.
*/
class UNITY_STORAGE_EXPORT InvalidArgumentException : public StorageException
{
public:
    InvalidArgumentException(QString const& error_message);
    ~InvalidArgumentException();

    virtual InvalidArgumentException* clone() const override;
    virtual void raise() const override;
};

/**
\brief Indicates a system error, such as failure to create a file or folder,
or any other (usually non-recoverable) kind of error that should not arise during normal operation.
*/
class UNITY_STORAGE_EXPORT ResourceException : public StorageException
{
public:
    ResourceException(QString const& error_message, int error_code);
    ~ResourceException();

    virtual ResourceException* clone() const override;
    virtual void raise() const override;

    int error_code() const noexcept;

private:
    int error_code_;
};

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
