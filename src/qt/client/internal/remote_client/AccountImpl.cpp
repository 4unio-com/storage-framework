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

#include <unity/storage/qt/client/internal/remote_client/AccountImpl.h>

#include "ProviderInterface.h"
#include <unity/storage/qt/client/internal/remote_client/Handler.h>
#include <unity/storage/qt/client/internal/remote_client/RootImpl.h>
#include <unity/storage/qt/client/internal/remote_client/RuntimeImpl.h>
#include <unity/storage/qt/client/Runtime.h>

#include <cassert>

using namespace std;

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

AccountImpl::AccountImpl(weak_ptr<Runtime> const& runtime,
                         QString const& bus_name,
                         QString const& object_path,
                         QString const& owner,
                         QString const& owner_id,
                         QString const& description)
    : AccountBase(runtime)
    , owner_(owner)
    , owner_id_(owner_id)
    , description_(description)
{
    auto rt_impl = dynamic_pointer_cast<RuntimeImpl>(runtime.lock()->p_);
    assert(rt_impl);
    provider_.reset(new ProviderInterface(bus_name, object_path, rt_impl->connection()));
}

QString AccountImpl::owner() const
{
    runtime();  // Throws if runtime was destroyed.
    return owner_;
}

QString AccountImpl::owner_id() const
{
    runtime();  // Throws if runtime was destroyed.
    return owner_id_;
}

QString AccountImpl::description() const
{
    runtime();  // Throws if runtime was destroyed.
    return description_;
}

QFuture<QVector<Root::SPtr>> AccountImpl::roots()
{
    try
    {
        runtime();  // Throws if runtime was destroyed.
    }
    catch (RuntimeDestroyedException const&)
    {
        return make_exceptional_future<QVector<Root::SPtr>>(RuntimeDestroyedException("Account::roots()"));
    }

    auto reply = provider_->Roots();

    auto process_reply = [this](decltype(reply) const& reply, QFutureInterface<QVector<Root::SPtr>>& qf)
    {
        try
        {
            this->runtime();
        }
        catch (RuntimeDestroyedException const& e)
        {
            qf.reportException(RuntimeDestroyedException("Account::roots()"));
            qf.reportFinished();
            return;
        }

        QVector<shared_ptr<Root>> roots;
        auto metadata = reply.value();
        for (auto const& md : metadata)
        {
            if (md.type != ItemType::root)
            {
                // TODO: log impossible item type here
                continue;  // LCOV_EXCL_LINE
            }
            auto root = RootImpl::make_root(md, public_instance_);
            roots.append(root);
        }
        roots_ = roots;
        qf.reportResult(roots);
        qf.reportFinished();
    };

    auto handler = new Handler<QVector<Root::SPtr>>(this, reply, process_reply);
    return handler->future();
}

shared_ptr<ProviderInterface> AccountImpl::provider() const noexcept
{
    return provider_;
}

}  // namespace local_client
}  // namespace internal
}  // namespace client
}  // namespace qt
}  // namespace storage
}  // namespace unity
