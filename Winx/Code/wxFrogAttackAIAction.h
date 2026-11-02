#pragma once
#include "wxAIAction.h"

namespace winx::reconstruction
{
    class wxAICommandRecordForAnalysis;
    struct wxFrogAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        std::array<std::uint8_t, 60> rawOwn3AC;
        wxAICommandRecordForAnalysis* borrowedCachedCommand3E8;
        std::array<std::uint8_t, 4> rawOwn3EC;
    };
    class wxFrogAttackAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x4CFF056C;
        using CapturedState = wxFrogAttackAIActionCapturedStateForAnalysis;
        enum class ProfileForAnalysis { Unqualified, PcCapturedStableCachedCommand };
        enum class ResultForAnalysis
        { Completed, UnqualifiedProfile, MissingCachedCommand, CommandNotAdmitted, UnqualifiedNotify, UnqualifiedHook };
        // Explicit supplied postconstruction state. This does not produce the
        // native cache, initialize PathFinder or own any encoded/borrowed data.
        explicit wxFrogAttackAIAction(const CapturedState&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxFrogAttackAIAction() override = default;
        wxFrogAttackAIAction(const wxFrogAttackAIAction&) = delete;
        wxFrogAttackAIAction& operator=(const wxFrogAttackAIAction&) = delete;
        wxFrogAttackAIAction(wxFrogAttackAIAction&&) = delete;
        wxFrogAttackAIAction& operator=(wxFrogAttackAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        ResultForAnalysis ExitForAnalysis() noexcept;
        void vfunc_34_ClearForAnalysis() noexcept override; // PC primary cell2C.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const CapturedState& CapturedStateForAnalysis() const noexcept { return state_; }
        // Explicit between-call input controls, not a recovered cache producer.
        CapturedState& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool RefuseHookForAnalysis() noexcept;
        CapturedState state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
    };
}
