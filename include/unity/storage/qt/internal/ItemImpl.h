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

#include <unity/storage/internal/ItemMetadata.h>
#include <unity/storage/qt/Account.h>
#include <unity/storage/qt/internal/AccountImpl.h>
#include <unity/storage/qt/internal/RuntimeImpl.h>
#include <unity/storage/qt/internal/StorageErrorImpl.h>
#include <unity/storage/qt/Item.h>

namespace unity
{
namespace storage
{
namespace qt
{
namespace internal
{

class AccountImpl;
class RuntimeImpl;

class ItemImpl : public std::enable_shared_from_this<ItemImpl>
{
public:
    ItemImpl();
    ItemImpl(storage::internal::ItemMetadata const& md,
             std::shared_ptr<AccountImpl> const& account);
    ItemImpl(ItemImpl const&) = default;
    ItemImpl(ItemImpl&&) = delete;
    ~ItemImpl() = default;
    ItemImpl& operator=(ItemImpl const&) = default;
    ItemImpl& operator=(ItemImpl&&) = delete;

    QString itemId() const;
    QString name() const;
    Account account() const;
    QString etag() const;
    Item::Type type() const;
    QVariantMap metadata() const;
    QDateTime lastModifiedTime() const;
    QList<QString> parentIds() const;

    ItemListJob* parents() const;
    ItemJob* copy(Item const& newParent, QString const& newName) const;
    ItemJob* move(Item const& newParent, QString const& newName) const;
    VoidJob* deleteItem() const;
    Uploader* createUploader(Item::ConflictPolicy policy, qint64 sizeInBytes) const;
    Downloader* createDownloader() const;
    ItemListJob* list() const;
    ItemListJob* lookup(QString const& name) const;
    ItemJob* createFolder(QString const& name) const;
    Uploader* createFile(QString const& name) const;
    IntJob* freeSpaceBytes() const;
    IntJob* usedSpaceBytes() const;

    bool operator==(ItemImpl const&) const;
    bool operator!=(ItemImpl const&) const;
    bool operator<(ItemImpl const&) const;
    bool operator<=(ItemImpl const&) const;
    bool operator>(ItemImpl const&) const;
    bool operator>=(ItemImpl const&) const;

    size_t hash() const;

    static Item make_item(QString const& method,
                          storage::internal::ItemMetadata const& md,
                          std::shared_ptr<AccountImpl> const& account);

    std::shared_ptr<RuntimeImpl> runtime() const;
    std::shared_ptr<AccountImpl> account_impl() const;

private:
    template<typename T, typename J>
    T* check_invalid_or_destroyed(QString const& method) const;

    template<typename T, typename J>
    T* check_copy_move_precondition(QString const& method, Item const& newParent, QString const& newName) const;

    bool is_valid_;
    storage::internal::ItemMetadata md_;
    std::shared_ptr<AccountImpl> account_;

    friend class unity::storage::qt::Item;
};

template<typename T, typename J>
T* ItemImpl::check_invalid_or_destroyed(QString const& method) const
{
    if (!is_valid_)
    {
        auto e = StorageErrorImpl::logic_error(method + ": cannot create job from invalid item");
        return J::make_job(e);
    }
    auto runtime = account_->runtime();
    if (!runtime || !runtime->isValid())
    {
        auto e = StorageErrorImpl::runtime_destroyed_error(method + ": Runtime was destroyed previously");
        return J::make_job(e);
    }
    return nullptr;
}

template<typename T, typename J>
T* ItemImpl::check_copy_move_precondition(QString const& method, Item const& newParent, QString const& newName) const
{
    auto invalid_job = check_invalid_or_destroyed<T, J>(method);
    if (invalid_job)
    {
        return invalid_job;
    }
    if (!newParent.isValid())
    {
        auto e = StorageErrorImpl::invalid_argument_error(method + ": newParent is invalid");
        return J::make_job(e);
    }
    if (newName.isEmpty())
    {
        auto e = StorageErrorImpl::invalid_argument_error(method + ": newName cannot be empty");
        return J::make_job(e);
    }
    if (account() != newParent.account())
    {
        auto e = StorageErrorImpl::logic_error(method + ": source and target must belong to the same account");
        return J::make_job(e);
    }
    if (newParent.type() == Item::Type::File)
    {
        auto e = StorageErrorImpl::logic_error(method + ": newParent cannot be a file");
        return J::make_job(e);
    }
    return nullptr;
}

}  // namespace internal
}  // namespace qt
}  // namespace storage
}  // namespace unity
