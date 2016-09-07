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

#include <unity/storage/common.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QFuture>
#pragma GCC diagnostic pop

#include <memory>

class QLocalSocket;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class File;

namespace internal
{

class UploaderBase : public QObject
{
public:
    UploaderBase(ConflictPolicy policy, int64_t size);
    UploaderBase(UploaderBase&) = delete;
    UploaderBase& operator=(UploaderBase const&) = delete;

    virtual std::shared_ptr<QLocalSocket> socket() const = 0;
    virtual QFuture<std::shared_ptr<File>> finish_upload() = 0;
    virtual QFuture<void> cancel() noexcept = 0;

    int64_t size() const;

protected:
    ConflictPolicy policy_;
    int64_t size_;
};

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
