#pragma once
#include "wxOpenGateStateHost.h"
namespace winx::reconstruction
{
    // Required borrowed graph/delivery boundary. Reusing the OpenGate packet
    // adapter is a host choice, not an assertion of native C++ inheritance.
    class wxPhysicalAttackStateHost : public wxOpenGateStateHost
    {
    public:
        // Native owner+24, possibly null. Reread separately on each event.
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Packet [271F,0,0,0,source,0,foot_left,flagWord]. Only the low
        // byte of flagWord is initialized (begin1/end0); upper24 bits are
        // native stack padding and must not be invented as zero.
        virtual void SendImpactNotificationForAnalysis(void* receiver,
            wxCharacterState& source, const char* boneName, bool begin) = 0;
    };
}
