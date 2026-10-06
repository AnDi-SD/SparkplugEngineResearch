#pragma once
#include "wxCharacterStateHost.h"
#include <string_view>
namespace winx::reconstruction
{
    class wxCharacterState;
    // Our borrowed foreign consumer, owner graph and event boundary.
    class wxSpiderAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual void SetConsumerRateForAnalysis(void* consumer, float rate) = 0;
        virtual std::uint8_t OwnerControlByte20ForAnalysis(void* owner) = 0;
        virtual std::string_view EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Notification 271F: zero metadata, source state, unnamed bool payload.
        // Native initializes only the low flag byte; upper bytes are padding.
        virtual void SendImpactNotificationForAnalysis(void* receiver,
            wxCharacterState& source, bool begin) = 0;
        // PC owner124->object140 vtable38; PS2 owner130->object14C slot40.
        virtual void TriggerOwnerActionForAnalysis(void* owner, std::uint32_t action) = 0;
    };
}
