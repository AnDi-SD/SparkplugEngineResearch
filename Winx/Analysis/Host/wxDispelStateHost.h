#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxDispelState;
    // Portable view of the eight original message words; not a native overlay.
    struct wxDispelStateMessageForAnalysis final
    {
        std::uint32_t code = 0x2806;
        std::uint32_t word04 = 0, word08 = 0, word0C = 0;
        const wxDispelState* source = nullptr;
        std::uint32_t word14 = 0;
        std::uint32_t word18 = 0x22, word1C = 1;
    };
    class wxDispelStateHost : public wxCharacterStateHost
    {
    public:
        // This is control+68; PC owner+12C / PS2 owner+138 selects control.
        virtual void ClearOwnerControlWord68ForAnalysis(void* owner) = 0;
        // The original manager pointer is optional; no lazy creation here.
        virtual bool HasNotificationManagerForAnalysis() const = 0;
        virtual void DispatchExitMessageForAnalysis(
            const wxDispelStateMessageForAnalysis& message) = 0;
    };
}
