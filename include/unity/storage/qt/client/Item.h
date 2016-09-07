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
#include <unity/storage/visibility.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QDateTime>
#include <QFuture>
#pragma GCC diagnostic pop

#include <memory>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{

class Folder;
class Root;

typedef QMap<QString, QVariant> MetadataMap;

namespace internal
{

class ItemBase;

namespace local_client
{

class UploadWorker;

}  // namespace local_client

namespace remote_client
{

class CopyHandler;
class ItemImpl;
class LookupHandler;
class MetadataHandler;

}  // namespace remote_client
}  // namespace internal

/**
\brief Base class for files and folders.
*/
class UNITY_STORAGE_EXPORT Item
{
public:
    /// @cond
    virtual ~Item();
    /// @endcond

    Item(Item&&);
    Item& operator=(Item&&);

    /**
    \brief Convenience type definition.
    */
    typedef std::shared_ptr<Item> SPtr;

    /**
    \brief Returns the native identifier used by the provider.
    */
    QString native_identity() const;

    /**
    \brief Returns the name of the file or folder.

    The returned name may not be the same as the name that was used to create the item because the provider
    may have changed it in some way (such as converting upper case characters to lower case).
    */
    QString name() const;

    /**
    \brief Returns the root folder for this item.

    If this item is a root, the returned pointer points at this item.
    */
    std::shared_ptr<Root> root() const;

    /**
    \brief Returns the type of the item.
    */
    ItemType type() const;

    /**
    \brief Returns a version identifier for the item.

    The version identifier changes each time the file is updated (possibly
    via some channel other than this API).
    */
    QString etag() const;

    /**
    \brief Returns metadata for the item.

    TODO: Needs a lot more doc. Explain standard and provider-specific metadata.
    */
    QVariantMap metadata() const;

    /**
    \brief Returns the time at which the item was last modified.
    */
    QDateTime last_modified_time() const;

    /**
    \brief Returns a list of parent folders of this item.
    \return A vector of parents. For a root, the returned vector is empty.
    \warn Depending on the provider, a single file or folder may have multiple
    parents. Do not assume that only a single parent will be returned, or that
    parents are returned in a particular order.
    */
    QFuture<QVector<std::shared_ptr<Folder>>> parents() const;

    /**
    \brief Returns the native identities of the parents of this item.
    \return A vector of parent identities. For a root, the returned vector is empty.
    \warn Depending on the provider, a single file or folder may have multiple
    parents. Do not assume that only a single parent ID will be returned, or that
    parent IDs are returned in a particular order.
    */
    QVector<QString> parent_ids() const;

    /**
    \brief Copies this item.

    Copying a folder recursively copies its contents.
    \param new_parent The new parent folder for the item. If the item is to be copied within
    its current folder, this parameter must designate the currently existing parent.
    \param new_name The new name for the file.
    \warn Do not rely on copy() to fail if an attempt is made to copy
    a file or folder to a destination name that is the same as that of an already existing file or folder.
    Depending on the cloud provider, it may be possible to have several folders with the same name.
    */
    QFuture<Item::SPtr> copy(std::shared_ptr<Folder> const& new_parent, QString const& new_name);

    /**
    \brief Renames and/or moves a file or folder.
    \param new_parent The new parent folder for the item. If the item is to be renamed within
    its current folder, this parameter must designate the currently existing parent.
    \param new_name The new name for the item.
    \warn Do not rely on move() to fail if an attempt is made to move
    a file or folder to a destination name that is the same as that of an already existing file or folder.
    Depending on the cloud provider, it may be possible to have several files or folders with the same name.
    \note It is not possible to move or rename the root folder.
    */
    QFuture<Item::SPtr> move(std::shared_ptr<Folder> const& new_parent, QString const& new_name);

    /**
    \brief Permamently deletes the item.
    \warning Deleting a folder recursively deletes its contents.
    */
    QFuture<void> delete_item();

    /**
    \brief Returns the time at which an item was created.
    \return If a provider does not support this method, the returned `QDateTime`'s `isValid()`
    method returns false.
    */
    QDateTime creation_time() const;

    /**
    \brief Returns provider-specific metadata.

    The contents of the returned map depend on the actual provider. This method is provided
    to allow applications to use provider-specific features that may not be
    supported by all providers.
    \return The returned map may be empty if a provider does not support this feature. If a provider
    supports it, the following keys are guaranteed to be present:
        - `native_provider_id` (string)
          A string that identifies the provider, such as "mCloud".
        - `native_provider_version` (string)
          A string that provides a version identifier.
    \warn Unless you know that your application will only be used with a specific provider,
    avoid using this method. If you do use provider-specific data, ensure reasonable fallback
    behavior for your application if it encounters a different provider that does not support
    a particular metadata item.
    // TODO: document where to find the list of metadata items for each concrete provider.
    */
    MetadataMap native_metadata() const;

    /**
    \brief Compares two items for equality.

    Equality comparison is deep, that is, it compares the native identities of two items, not
    their `shared_ptr` values.
    \note If you retrieve the same item more than once (such as by calling Root::get() twice
    with the same file ID) and then perform an upload using one of the two file handles, the
    files still have the same identity after the upload. However, the etag() values of the two
    file handles differ after the upload. Despite this, equal_to() still returns `true` for
    the two files, that is, the ETags are ignored for equality comparison.
    \return `!this->native_identity() == other->native_identity()`
    \throws DeletedException if `this` or `other` have been deleted.
    */
    bool equal_to(Item::SPtr const& other) const noexcept;

protected:
    Item(internal::ItemBase* p) UNITY_STORAGE_HIDDEN;

    std::shared_ptr<internal::ItemBase> p_;

    friend class internal::local_client::UploadWorker;
    friend class internal::remote_client::CopyHandler;
    friend class internal::remote_client::ItemImpl;
    friend class internal::remote_client::LookupHandler;
    friend class internal::remote_client::MetadataHandler;
};

}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
