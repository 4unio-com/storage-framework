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

#include <unity/storage/qt/client/Uploader.h>

#include <unity/storage/qt/client/File.h>
#include <unity/storage/qt/client/internal/UploaderBase.h>

using namespace std;

class QLocalSocket;

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

Uploader::Uploader(internal::UploaderBase* p)
    : p_(p)
{
}

Uploader::~Uploader() = default;

std::shared_ptr<QLocalSocket> Uploader::socket() const
{
    return p_->socket();
}

int64_t Uploader::size() const
{
    return p_->size();
}

QFuture<shared_ptr<File>> Uploader::finish_upload()
{
    return p_->finish_upload();
}

QFuture<void> Uploader::cancel()
{
    return p_->cancel();
}

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
