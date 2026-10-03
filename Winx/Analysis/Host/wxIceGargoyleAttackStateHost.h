#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    class wxIceGargoyleAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        virtual void* OwnerEntityField140ForAnalysis(void* owner) = 0;
        // PC slot38 / PS2slot40; called only for nonnull controller.
        virtual void InvokeControllerSlot38ForAnalysis(void* controller,
            std::uint32_t argument) = 0;
        // Packet271F,0,0,0,source,0,name,flagWord. Name is the stable
        // literal hand_right. Only the low flag byte is known; upper24
        // padding bits of the flag word are not initialized by the game.
        virtual void SendNamedFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, const char* name, bool flag) = 0;
    };
}
