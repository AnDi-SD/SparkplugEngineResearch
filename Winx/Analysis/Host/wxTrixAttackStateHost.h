#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    class wxTrixAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Borrowed receiver virtual dispatch. Packet[code,0,0,0,source,0,0,0].
        virtual void SendZeroNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code) = 0;
        // Packet277D: only payload0 low byte is known (begin1/end0),
        // upper24padding bits are uninitialized; payload1 is zero.
        virtual void SendCirclesNotificationForAnalysis(void* receiver,
            wxCharacterState& source, bool begin) = 0;
    };
}
