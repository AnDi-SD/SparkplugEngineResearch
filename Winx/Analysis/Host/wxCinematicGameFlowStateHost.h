#pragma once

#include "wxGameFlowStateHost.h"
#include <array>
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxCinematicGameFlowState;
    // Storage binding for the82 raw flag words at PC741374+8*i. Names and
    // movie loading stay outside this contract; the source clears only bit0.
    using wxCinematicFlagsForAnalysis = std::array<std::uint32_t,82>;

    class wxCinematicGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        virtual wxCinematicFlagsForAnalysis& CinematicFlagsForAnalysis() = 0;
        // Required captured native32 receiver binding. Exit's scratch word is
        // initialized by PUSH ECX, then only its low byte is replaced by E0.
        // The portable object's address is never used as the native word.
        virtual std::optional<std::uint32_t> CinematicNativeReceiverWordForAnalysis(
            const wxCinematicGameFlowState&) const noexcept = 0;
        // Borrowed synchronous foreign-input boundary. member1C is distinct
        // from the parent's member18. These callbacks do not recover UserInput
        // or InputDevicePC, and must not replace a Cinematic decision or store.
        virtual void* ResolveCinematicInputOwnerForAnalysis() = 0;
        virtual void* ReadCinematicInput1CForAnalysis(void* owner) = 0;
        virtual void ResetCinematicInputForAnalysis(void* input) = 0;
        virtual void SetCinematicInputModeForAnalysis(void* input,std::uint32_t mode) = 0;
    };
}
