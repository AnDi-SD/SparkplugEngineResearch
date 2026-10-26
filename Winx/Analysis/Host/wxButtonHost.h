#pragma once

// Our boundary for foreign managers called by wxButton. The button's field
// initialization, bounded string copy and selection logic stay in common code.
#include "Analysis/Host/spGUIHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    class wxButton;

    class wxButtonHost : public sparkplug::reconstruction::spGUIHost
    {
    public:
        virtual void RemoveWxButtonFromEngineManagerForAnalysis(wxButton&) = 0;
        virtual void RegisterWxButtonForAnalysis(wxButton&) = 0;
        virtual void DestroyWxButtonForAnalysis(wxButton&) noexcept = 0;
        // Native copy resolves the external clone manager and calls5B7A00
        // with classID only. Its return value is not read by wxButton.
        virtual void CompleteWxButtonCopyForAnalysis(std::uint32_t classID) const = 0;
    };
}
