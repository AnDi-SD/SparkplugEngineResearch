#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    // Own PC/PS2 state hooks; physical native object is 44 bytes (hex).
    class wxBirdMovingState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1D533B00;
        wxBirdMovingState() noexcept = default;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        using wxCharacterState::vfunc_1C;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        void vfunc_3C(const void* event) override;
        [[nodiscard]] std::uint32_t GetCycleCountForAnalysis() const noexcept { return cycleCount_; }
        [[nodiscard]] bool GetNeedsPlaybackForAnalysis() const noexcept { return needsPlayback_; }
        // Explicit fixture/adapter state, not an original game method.
        void SetCycleForAnalysis(std::uint32_t count, bool needsPlayback) noexcept
        { cycleCount_=count; needsPlayback_=needsPlayback; }
    private:
        std::uint32_t cycleCount_ = 0; // native word3C; exact meaning beyond use unknown
        bool needsPlayback_ = true; // native byte40
    };
}
