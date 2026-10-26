#pragma once

// Standalone recovery of PC5D6E70 and helper5D6BE0. Typed Menu callers retain
// the physical GameFlow prefix; the lost C++ declaration owner is unknown.
// Placing the operation here does not attribute another native method/class.
#include "wxGameFlowState.h"
#include "Analysis/Host/wxGameFlowNavigationHost.h"
#include <optional>

namespace winx::reconstruction
{
    struct wxGameFlowDirectionBoundsForAnalysis final
    {
        float central = 0x1.921FB6p-1f; // PC742EC4
        float helper = 0x1.921FB6p-1f;  // independent PC740AC4
    };

    // Finite float32 angle and positive finite bounds. Products3*q remain
    // unspilled, as in the PC x87 helpers. Direction numbers are branch IDs.
    [[nodiscard]] std::optional<unsigned> ClassifyGameFlowDirectionPCForAnalysis(
        float angle, wxGameFlowDirectionBoundsForAnalysis bounds = {});

    // Valid PC-constructed state, stable owned timer and its clock domain are
    // required. Angle/bounds must satisfy Classify's declared finite domain;
    // invalid angle refusal can follow timer/window work, not a native return.
    // Foreign
    // handles/buffers stay borrowed throughout this synchronous operation.
    // A host guard stops4096 failed hops; native code only stops on returning
    // to the initial widget after at least20 failures, so other cycles can hang.
    // Guard failure preserves completed work and is not a native return.
    void NavigateGameFlowPCForAnalysis(wxGameFlowState&, std::uint32_t view,
        wxGameFlowNavigationHost&,
        const sparkplug::reconstruction::spTaskTimer::ClockSourceForAnalysis&,
        wxGameFlowDirectionBoundsForAnalysis bounds = {});
}
