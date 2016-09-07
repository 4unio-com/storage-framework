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

#pragma once

#include <unity/storage/provider/UploadJob.h>
#include <unity/storage/visibility.h>

#include <string>
#include <memory>

namespace unity
{
namespace storage
{
namespace provider
{

namespace internal
{
class TempfileUploadJobImpl;
}

class UNITY_STORAGE_EXPORT TempfileUploadJob : public UploadJob
{
public:
    TempfileUploadJob(std::string const& upload_id);
    virtual ~TempfileUploadJob();

    std::string file_name() const;

    // This function should be called from your finish()
    // implementation to read the remaining data from the socket.  If
    // the client has not closed the socket as expected, LogicError
    // will be thrown.
    void drain();

protected:
    TempfileUploadJob(internal::TempfileUploadJobImpl *p) UNITY_STORAGE_HIDDEN;
};

}
}
}
