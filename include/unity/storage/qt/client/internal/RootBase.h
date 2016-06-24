#pragma once

#include <unity/storage/qt/client/Root.h>
#include <unity/storage/qt/client/internal/FolderBase.h>

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
namespace remote_client
{

class ItemImpl;
class LookupHandler;

}  // namespace remote_client

class RootBase : public virtual FolderBase
{
public:
    RootBase(QString const& identity, std::weak_ptr<Account> const& account);

    Account* account() const;
    virtual QFuture<int64_t> free_space_bytes() const = 0;
    virtual QFuture<int64_t> used_space_bytes() const = 0;
    virtual QFuture<Item::SPtr> get(QString native_identity) const = 0;

    static std::shared_ptr<Root> make_root(QString const& identity, std::weak_ptr<Account> const& account);

protected:
    std::weak_ptr<Account> account_;

    friend class remote_client::ItemImpl;
    friend class remote_client::LookupHandler;
};

}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
