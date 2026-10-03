#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    // Our borrowed-object adapter. A partial payload word deliberately carries
    // its known-bit mask; uninitialized original padding is not declared zero.
    struct wxKnutNotificationWordForAnalysis final
    {
        std::uint32_t value;
        std::uint32_t knownMask;
    };
    class wxKnutAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual std::uint8_t OwnerEntityMotionFlag1FForAnalysis(void* owner) = 0;
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        virtual void* GameCoreField2B4ForAnalysis() = 0;
        virtual void* OwnerEntityControllerForAnalysis(void* owner) = 0;
        virtual void InvokeControllerSlot38ForAnalysis(void* controller, std::uint32_t argument) = 0;
        virtual void InvokeControllerSlot3CForAnalysis(void* controller, std::uint32_t first, std::uint32_t second) = 0;
        // Packet [code,0,0,0,source,0,payload0,payload1]. Only masked bits
        // of each payload are established by the original event body.
        virtual void SendNotificationForAnalysis(void* receiver, wxCharacterState& source,
            std::uint32_t code, wxKnutNotificationWordForAnalysis payload0,
            wxKnutNotificationWordForAnalysis payload1) = 0;
    };
}
