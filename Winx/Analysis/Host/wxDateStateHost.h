#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    class wxDateStateHost : public wxCharacterStateHost
    {
    public:
        // Borrowed owner+24 word. No original source type is inferred.
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // PC524200 / PS2310B80,311050,310DE0 send selector and the owner word
        // as the two packet payload words. Dispatch/delivery remains external.
        virtual void SendExitNotificationForAnalysis(wxCharacterState& source,
            std::uint32_t code, std::uint32_t parameter,
            std::uint32_t selector, void* ownerWord) = 0;
    };
}
