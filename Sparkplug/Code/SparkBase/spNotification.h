#pragma once

// Original PC435D30 is an observed shared sender helper. Its owning class,
// source/header path and source-level declaration have not been recovered.
// This standalone operation preserves its eight logical words. Pointer-sized
// portable fields are NOT the original 32-bit byte ABI or a registered class.
#include "Analysis/Host/spNotificationHost.h"
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct spNotificationForAnalysis final
    {
        std::uint32_t code=0,word04=0,word08=0,word0C=0;
        const void* sender=nullptr;
        std::uint32_t word14=0;
        std::uintptr_t payload=0,tail=0;
    };

    // The frame and payload are synchronously borrowed. No reference is
    // retained/released. Null target still builds the frame, then skips dispatch.
    // Callers must read mutable source fields into locals in the native order
    // before invoking this helper; C++ argument evaluation order is not assumed.
    inline void SendDirectedNotificationForAnalysis(spNotificationHost& host,
        const void* sender,void* target,std::uint32_t code,
        std::uintptr_t payload,std::uintptr_t tail)
    {
        spNotificationForAnalysis notification;
        notification.code=code;notification.sender=sender;
        notification.payload=payload;notification.tail=tail;
        if(target)host.DispatchNotificationForAnalysis(target,notification);
    }
}
