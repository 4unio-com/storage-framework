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

#include <unity/storage/qt/client/Folder.h>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Account;
class Item;

namespace internal
{

class RootBase;

namespace local_client
{

class RootImpl;

}  // namespace local_client

namespace remote_client
{

class RootImpl;

}  // namespace remote_client
}  // namespace internal

/**
\brief Class that represents a root folder.
*/
class UNITY_STORAGE_EXPORT Root final : public Folder
{
public:
    // @cond
    virtual ~Root();
    /// @endcond

    Root(Root&&);
    Root& operator=(Root&&);

    typedef std::shared_ptr<Root> SPtr;

    /**
    \brief Returns the account for this root.
    */
    std::shared_ptr<Account> account() const;

    QFuture<int64_t> free_space_bytes() const;
    QFuture<int64_t> used_space_bytes() const;

    QFuture<Item::SPtr> get(QString native_identity) const;

    // TODO: Do we need a method to get lots of things?

private:
    Root(internal::RootBase*) UNITY_STORAGE_HIDDEN;

    friend class internal::local_client::RootImpl;
    friend class internal::remote_client::RootImpl;
};

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
