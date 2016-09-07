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

#include <unity/storage/provider/TempfileUploadJob.h>
#include <unity/storage/provider/internal/TempfileUploadJobImpl.h>

using namespace std;

namespace unity
{
namespace storage
{
namespace provider
{

TempfileUploadJob::TempfileUploadJob(internal::TempfileUploadJobImpl *p)
    : UploadJob(p)
{
}

TempfileUploadJob::TempfileUploadJob(string const& upload_id)
    : TempfileUploadJob(new internal::TempfileUploadJobImpl(upload_id))
{
}

TempfileUploadJob::~TempfileUploadJob() = default;

void TempfileUploadJob::drain()
{
    static_cast<internal::TempfileUploadJobImpl*>(p_)->drain();
}

string TempfileUploadJob::file_name() const
{
    return static_cast<internal::TempfileUploadJobImpl*>(p_)->file_name();
}

}
}
}
