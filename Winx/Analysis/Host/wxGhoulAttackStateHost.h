#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    class wxGhoulAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // PC owner+124 -> entity+12C; PS2 +130 -> +138, borrowed.
        virtual void* OwnerEntityField12CForAnalysis(void* owner) = 0;
        // PC513A30 writes controller words1C8/1CC/1D0=(0,value,0), byte1D4=1.
        // PS2 inline uses words1D4/1D8/1DC and byte1E0. Neighboring bytes
        // remain unchanged. This external controller service is mandatory.
        virtual void InvokeControllerFloatServiceForAnalysis(void* controller, float value) = 0;
        // PC owner+124 -> entity+140; PS2 +130 -> +14C. Valid object required.
        virtual void* OwnerEntityField140ForAnalysis(void* owner) = 0;
        virtual void InvokeControllerSlot38ForAnalysis(void* controller, std::uint32_t argument) = 0;
        // Packet271F,0,0,0,source,0,name,flagWord. Only low flag byte is
        // known; upper24padding is not initialized by the original.
        virtual void SendNamedFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, const char* name, bool flag) = 0;
        // Packet2754,0,0,0,source,0,0,0, borrowed receiver dispatch.
        virtual void SendThrowNotificationForAnalysis(void* receiver, wxCharacterState& source) = 0;
    };
}
