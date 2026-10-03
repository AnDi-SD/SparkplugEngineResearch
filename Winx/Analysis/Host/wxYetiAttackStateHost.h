#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    class wxYetiAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Packet2755,0,0,0,source,0,0,0; external borrowed receiver dispatch.
        virtual void SendBackspikeNotificationForAnalysis(void* receiver,
            wxCharacterState& source) = 0;
        // PC owner+124 -> entity+140; PS2 owner+130 -> entity+14C.
        // Event branches require a valid controller: original has no null guard.
        virtual void* OwnerEntityField140ForAnalysis(void* owner) = 0;
        // PC vslots38/3C (PS2+40/+44); original names remain unknown.
        virtual void InvokeControllerSlot38ForAnalysis(void* controller,
            std::uint32_t argument) = 0;
        virtual void InvokeControllerSlot3CForAnalysis(void* controller,
            std::uint32_t argument0, std::uint32_t argument1) = 0;
    };
}
