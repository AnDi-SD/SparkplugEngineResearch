#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxAIAttackStateStorageHost.h"
#include <cstddef>

namespace winx::reconstruction
{
    // A captured raw-state facade plus the existing portable wxAIAction base.
    // The raw 988B snapshot is inert data, not this C++ object's memory
    // layout. Native same-receiver construction proves the separate PC prefix;
    // native containers, encoded pointers and ownership are not transferred.
    class wxSpiderAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x71237CFA;
        static constexpr std::size_t CapturedBytesForAnalysis = 988;
        using CapturedState = std::array<std::uint8_t, CapturedBytesForAnalysis>;
        using ProfileForAnalysis = wxAIAttackStateProfileForAnalysis;
        using ResultForAnalysis = wxAIAttackStateResultForAnalysis;
        using AccessForAnalysis = wxAIAttackStateAccessForAnalysis;
        explicit wxSpiderAttackAIAction(const CapturedState&, void* borrowedOwner,
            wxAIAttackStateStorageHost* storage = nullptr,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxSpiderAttackAIAction() override = default;
        wxSpiderAttackAIAction(const wxSpiderAttackAIAction&) = delete;
        wxSpiderAttackAIAction& operator=(const wxSpiderAttackAIAction&) = delete;
        wxSpiderAttackAIAction(wxSpiderAttackAIAction&&) = delete;
        wxSpiderAttackAIAction& operator=(wxSpiderAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        // Our method name and admission result only. The helper's original
        // C++ return type is unestablished; native EAX remains diagnostic.
        ResultForAnalysis StateGateForAnalysis() noexcept;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const CapturedState& CapturedStateForAnalysis() const noexcept { return state_; }
        CapturedState& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
        const std::array<AccessForAnalysis, 12>& LastAccessesForAnalysis() const noexcept { return accesses_; }
        std::size_t LastAccessCountForAnalysis() const noexcept { return accessCount_; }
    private:
        std::uint32_t ReadWordForAnalysis(std::size_t) const noexcept;
        void WriteWordForAnalysis(std::size_t, std::uint32_t) noexcept;
        void WriteByteForAnalysis(std::size_t, std::uint8_t) noexcept;
        void PointerReadForAnalysis(const void*, std::uint32_t, const void*) noexcept;
        void ValueReadForAnalysis(const void*, std::uint32_t, std::uint8_t, std::uint32_t) noexcept;
        void ValueWriteForAnalysis(const void*, std::uint32_t, std::uint8_t, std::uint32_t) noexcept;
        bool RefuseHookForAnalysis() noexcept;
        CapturedState state_;
        wxAIAttackStateStorageHost* const storage_; // borrowed, never owned
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        std::array<AccessForAnalysis, 12> accesses_{};
        std::size_t accessCount_ = 0;
    };
}
