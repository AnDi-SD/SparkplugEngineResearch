#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxDialogueState;
    class wxDialogueStateHost : public wxCharacterStateHost
    {
    public:
        // PC40EC00 / PS21007A0: source remains this state; packet delivery
        // and the meaning of the parameter are external game operations.
        virtual void SendEntryMessageForAnalysis(wxDialogueState& source,
            std::uint32_t code, std::uint32_t parameter,
            std::uint32_t firstWord, std::uint32_t secondWord) = 0;
        virtual std::uint32_t GameStateForAnalysis() const = 0;
    };
}
