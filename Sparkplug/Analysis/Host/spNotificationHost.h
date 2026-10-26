#pragma once

namespace sparkplug::reconstruction
{
    struct spNotificationForAnalysis;

    // Our synchronous boundary for the native receiver's notification slot.
    // target is an opaque external handle, not an asserted C++ object/owner.
    class spNotificationHost
    {
    public:
        virtual ~spNotificationHost() = default;
        virtual void DispatchNotificationForAnalysis(void* target,
            const spNotificationForAnalysis& notification) = 0;
    };
}
