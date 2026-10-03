#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxShadowBeastDefenseStateHost : public wxCharacterStateHost
    {
    public:
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Packet [code,0,0,0,source,0,0x30,0]; every payload bit is known.
        virtual void SendDefenseNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code) = 0;
        // PC vslot34 / PS2 vslot3C, borrowed controller stored in state40.
        virtual void InvokeDefenseControllerForAnalysis(void* controller,
            std::uint32_t first, std::uint32_t second) = 0;
    };
}
