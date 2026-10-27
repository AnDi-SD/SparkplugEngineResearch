#pragma once

// Inferred source path. This is a portable component, not the native ABI.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxPerceptionHost.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace winx::reconstruction
{
    class wxGameTimer;
    enum class wxPerceptionPcRefreshResultForAnalysis
    {
        PausedReturnFalse,
        OwnerNullReturnTrue,
        UnqualifiedOwnerScan,
        UnqualifiedNativeDeadline,
    };
    enum class wxPerceptionNumericProfileForAnalysis
    {
        ExactSmallInteger,
        PcFiniteNearest64,
    };

    struct wxPerceptionObservedStateForAnalysis final
    {
        std::array<std::uint32_t, 3> words24{};
        // Native construction leaves +30/+34/+38 unwritten.
        std::optional<std::array<std::uint32_t, 3>> scratchWords30;
        std::uint32_t word3C = 0;
        std::uint8_t byte40 = 0;
        std::uint32_t word44 = 0;
        std::uint8_t byte48 = 0;
        std::uint32_t word4C = 0x49742400;
        std::uint32_t word50 = 0x3F3504F3; // observed PC cosine result
        std::uint32_t word54 = 0;
        std::uint32_t word58 = 0;
        std::uint32_t word5C = 0;
        std::uint32_t word60 = 0x3F800000;
        std::uint8_t byte64 = 0;
        std::uint8_t byte65 = 0;
        std::uintptr_t opaque68 = 0;
        std::uintptr_t opaque6C = 0;
        std::uint8_t byte70 = 0;
        std::uint32_t word74 = 0x7F7FFFFF;
        std::uint8_t byte78 = 0;
    };

    class wxPerception final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3A3277D4;
        wxPerception() = default;
        ~wxPerception() override;
        wxPerception(const wxPerception&) = delete;
        wxPerception& operator=(const wxPerception&) = delete;

        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;

        // Analytical binding only. Original list population/attach is open.
        // All targets, the owner and the host are borrowed; the host outlives us.
        void BindForAnalysis(wxPerceptionHost&, void* owner,
            std::vector<void*> orderedTargets);
        void ResetForAnalysis() noexcept;

        // PC4F1910 / PS236EBC0 control flow. Default arithmetic is the exact
        // common integer domain [-1024,1024], excluding negative zero.
        // Explicit PcFiniteNearest64 models the PC's finite x87 control037F
        // operation/store order, including signed zeros and subnormals.
        // That PC profile makes no claim about EE floating-point behavior.
        [[nodiscard]] void* FindNearestForAnalysis(wxPerceptionNumericProfileForAnalysis
            profile = wxPerceptionNumericProfileForAnalysis::ExactSmallInteger);

        // PC4F1F30/4F1C70 with an already constructed actual wxGameTimer.
        // Paused returns native false without mutation. Unpaused handles the
        // literal68/6C expiry and clears40; null owner then returns native true.
        // An attached owner stops before the unqualified Scene/Asset scan.
        [[nodiscard]] wxPerceptionPcRefreshResultForAnalysis RefreshBoundedPcForAnalysis(
            wxGameTimer&) noexcept;

        wxPerceptionObservedStateForAnalysis& StateForAnalysis() noexcept { return state_; }
        const wxPerceptionObservedStateForAnalysis& StateForAnalysis() const noexcept { return state_; }
        void* OwnerForAnalysis() const noexcept { return owner_; }
        const std::vector<void*>& TargetsForAnalysis() const noexcept { return targets_; }

    private:
        wxPerceptionHost* host_ = nullptr;
        void* owner_ = nullptr;
        std::vector<void*> targets_;
        wxPerceptionObservedStateForAnalysis state_;
    };
}
