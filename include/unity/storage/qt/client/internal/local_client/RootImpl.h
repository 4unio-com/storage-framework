#pragma once

#include <unity/storage/qt/client/internal/RootBase.h>
#include <unity/storage/qt/client/internal/local_client/FolderImpl.h>

namespace unity
{
namespace storage
{
namespace qt
{
namespace client
{
namespace internal
{
namespace local_client
{

class RootImpl : public virtual RootBase, public virtual FolderImpl
{
public:
    RootImpl(QString const& identity, std::weak_ptr<Account> const& account);

    virtual QString name() const override;
    virtual QFuture<QVector<std::shared_ptr<Folder>>> parents() const override;
    virtual QVector<QString> parent_ids() const override;
    virtual QFuture<void> delete_item() override;

    virtual QFuture<int64_t> free_space_bytes() const override;
    virtual QFuture<int64_t> used_space_bytes() const override;
    virtual QFuture<Item::SPtr> get(QString native_identity) const override;

    static std::shared_ptr<Root> make_root(QString const& identity, std::weak_ptr<Account> const& account);
};

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity