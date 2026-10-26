#pragma once

#include "spNotification.h"
#include "Analysis/Host/spBroadcastNotificationHost.h"

namespace sparkplug::reconstruction
{
    enum class spBroadcastNotificationResultForAnalysis
    {
        Completed,
        UnqualifiedManagerProvider,
    };

    // PC40EC00 is a shared packet builder/dispatch operation. Its original
    // C++ owner and declaration are unknown; this is not a registered class.
    // All pointer fields/frame storage are synchronously borrowed. Native
    // subscribers observe one shared mutable frame in pointer-tree order.
    // Changing subscriptions or reentering their dispatch is unqualified.
    // The result enum describes OUR admission guard, not a native return value.
    [[nodiscard]] spBroadcastNotificationResultForAnalysis SendBroadcastNotificationForAnalysis(
        const void* sender, std::uint32_t code, std::uint32_t key,
        std::uintptr_t payload, std::uintptr_t tail,
        const spSubscriptionManagerFactoryForAnalysis& createManager = {});
}
