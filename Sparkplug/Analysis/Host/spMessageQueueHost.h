#pragma once

// Our native-address translation and application-owned singleton boundary.
// Neither interface replaces the Queue's dispatch or linked-list algorithms.
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spBaseObject;
    class spSubscriptionManager;

    struct spMessageQueueReceiverBindingForAnalysis final
    {
        std::uint32_t token = 0;
        spBaseObject* receiver = nullptr;
    };

    class spMessageQueueHost
    {
    public:
        virtual ~spMessageQueueHost() = default;

        // Called only when the real shared manager singleton is absent. The
        // application constructs/owns a real spSubscriptionManager, publishes
        // it through that class's constructor, and keeps it alive throughout
        // dispatch. Queue never owns/deletes this manager or the provider.
        virtual spSubscriptionManager& CreateSubscriptionManagerForAnalysis() = 0;
    };
}
