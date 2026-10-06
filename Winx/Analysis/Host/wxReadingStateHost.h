#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    // Our boundary for the global manager word and notification receivers.
    class wxReadingStateHost : public wxCharacterStateHost
    {
    public:
        // PC singleton755294->1B0; PS2 GP-4424->1B0. Original meaning open.
        virtual std::uint32_t GlobalWord1B0ForAnalysis() = 0;
        virtual void* MainReceiverForAnalysis() = 0;
        // Filtered27BA, filterE, zero payload.
        virtual void SendReadingEntryForAnalysis(wxCharacterState& source) = 0;
        // Directed2737, bool low byte then zero word. Upper bool bytes padding.
        virtual void SendReadingExitForAnalysis(void* receiver,
            wxCharacterState& source, bool firstPhase) = 0;
    };
}
