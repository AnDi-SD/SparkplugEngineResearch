#include "spSubscriptionManager.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSubscriptionManager()
        {
            return std::make_unique<spSubscriptionManager>();
        }

        const spRTTIRecord SubscriptionManagerRecord{
            spSubscriptionManager::ClassID,
            spBaseObject::ClassID,
            "spSubscriptionManager",
            &spBaseObject::StaticRTTI(),
            &CreateSubscriptionManager,
            nullptr,
        };
    }

    spSubscriptionManager* spSubscriptionManager::instance_ = nullptr;

    spSubscriptionManager::spSubscriptionManager() noexcept
    {
        instance_ = this;
    }

    spSubscriptionManager::~spSubscriptionManager()
    {
        instance_ = nullptr;
    }

    const spRTTIRecord& spSubscriptionManager::StaticRTTI() noexcept
    {
        return SubscriptionManagerRecord;
    }

    spSubscriptionManager* spSubscriptionManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spSubscriptionManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSubscriptionManager>();
        manager.RegisterClone(*this, *clone);
        // Native clone creates an empty manager and invokes the inherited
        // empty copy slot; the subscription graph is runtime-only.
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spSubscriptionManager::vfunc_18() const noexcept
    {
        return SubscriptionManagerRecord;
    }

    bool spSubscriptionManager::SubscribeForAnalysis(
        const std::uint32_t key,
        spBaseObject& subscriber)
    {
        auto& group = subscriptions_[key];
        if (std::find(group.begin(), group.end(), &subscriber) != group.end())
        {
            return false;
        }
        group.push_back(&subscriber);
        return true;
    }

    bool spSubscriptionManager::UnsubscribeForAnalysis(
        const std::uint32_t key,
        spBaseObject& subscriber)
    {
        const auto groupIterator = subscriptions_.find(key);
        if (groupIterator == subscriptions_.end())
        {
            return false;
        }

        auto& group = groupIterator->second;
        const auto subscriberIterator =
            std::find(group.begin(), group.end(), &subscriber);
        if (subscriberIterator == group.end())
        {
            return false;
        }
        group.erase(subscriberIterator);
        if (group.empty())
        {
            subscriptions_.erase(groupIterator);
        }
        return true;
    }

    std::size_t spSubscriptionManager::DispatchForAnalysis(
        const std::uint32_t key,
        const void* const notification)
    {
        const auto groupIterator = subscriptions_.find(key);
        if (groupIterator == subscriptions_.end())
        {
            return 0;
        }

        std::size_t delivered = 0;
        // std::list retains native iterator stability if a callback removes a
        // different subscriber; advance before invoking the virtual target.
        auto& group = groupIterator->second;
        for (auto iterator = group.begin(); iterator != group.end();)
        {
            auto* const subscriber = *iterator++;
            if (subscriber != nullptr)
            {
                subscriber->vfunc_0C(notification);
                ++delivered;
            }
        }
        return delivered;
    }

    std::size_t spSubscriptionManager::GetGroupCountForAnalysis() const noexcept
    {
        return subscriptions_.size();
    }

    std::size_t spSubscriptionManager::GetSubscriberCountForAnalysis(
        const std::uint32_t key) const noexcept
    {
        const auto iterator = subscriptions_.find(key);
        return iterator == subscriptions_.end() ? 0 : iterator->second.size();
    }
}
