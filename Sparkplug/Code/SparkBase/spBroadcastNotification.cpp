#include "spBroadcastNotification.h"
#include "spSubscriptionManager.h"

namespace sparkplug::reconstruction
{
    spBroadcastNotificationResultForAnalysis SendBroadcastNotificationForAnalysis(
        const void* sender, const std::uint32_t code, const std::uint32_t key,
        const std::uintptr_t payload, const std::uintptr_t tail,
        const spSubscriptionManagerFactoryForAnalysis& createManager)
    {
        spNotificationForAnalysis notification;
        notification.code = code;
        notification.word0C = key;
        notification.tail = tail;
        auto* manager = spSubscriptionManager::GetInstance();
        notification.sender = sender;
        notification.payload = payload;
        if (!manager)
        {
            if (!createManager)
                return spBroadcastNotificationResultForAnalysis::UnqualifiedManagerProvider;
            auto& created = createManager();
            manager = spSubscriptionManager::GetInstance();
            if (manager != &created)
                return spBroadcastNotificationResultForAnalysis::UnqualifiedManagerProvider;
        }
        (void)manager->DispatchForAnalysis(key, &notification);
        return spBroadcastNotificationResultForAnalysis::Completed;
    }
}
