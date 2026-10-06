#pragma once

// Inferred declaration path. Scalar/word names are analytical: the original
// names of these audio parameters have not been recovered.
#include "spNode.h"
#include "Analysis/Host/spAudioSoundHost.h"

namespace sparkplug::reconstruction
{
    class spAudioSound final : public spNode
    {
    public:
        static constexpr spClassID ClassID = 0x33283899;
        struct StateForAnalysis final
        {
            // PC B4..F4 / PS2 C0..100. Unknown fields retain exact word bits.
            std::array<std::uint32_t,17> words{0,0,0,0x447A0000,0x3F800000,0,0,0,
                0x3F800000,0,0,0,0,0,0,0,0};
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native Node vslot0C copies its prefix, leaving these tail words alone.
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        void SetStateForAnalysis(const StateForAnalysis& value) noexcept { state_ = value; }
        [[nodiscard]] float GetScalarForAnalysis() const noexcept;
        // PC4A2AD0 arithmetic uses cached AudioManager+1C+group*4. Group zero
        // returns the raw scalar and never accesses that manager.
        [[nodiscard]] bool GetCombinedScalarForAnalysis(const host::spAudioSoundHost&,
            float& output, std::string* error = nullptr) const;
    private:
        StateForAnalysis state_{};
    };
}
