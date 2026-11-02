#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxMosquitoAttackAIActionHost.h"

namespace winx::reconstruction
{
    struct wxMosquitoAttackAIActionCapturedStateForAnalysis final
    {
        void* borrowedTarget3A8;
        // Captured PC3AC..3CB; encoded pointers remain opaque input bytes.
        std::array<std::uint8_t,32> ownBytes3AC;
    };
    enum class wxMosquitoAttackAIActionResultForAnalysis
    {
        Completed,UnqualifiedProfile,MissingOwner,MissingCharacter,
        MissingCommand,MissingCommandCell,MissingController,
        UnqualifiedNotify,UnqualifiedHook,
    };
    class wxMosquitoAttackAIAction final:public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x4FA810CA;
        enum class ProfileForAnalysis{Unqualified,PcCapturedStableBorrowedExit};
        // Explicit captured-data adapter; no native constructor or ownership
        // graph is reconstructed. Host and all borrowed records outlive this.
        wxMosquitoAttackAIAction(wxMosquitoAttackAIActionHost&,
            const wxMosquitoAttackAIActionCapturedStateForAnalysis&,
            ProfileForAnalysis=ProfileForAnalysis::Unqualified) noexcept;
        ~wxMosquitoAttackAIAction() override=default;
        wxMosquitoAttackAIAction(const wxMosquitoAttackAIAction&)=delete;
        wxMosquitoAttackAIAction& operator=(const wxMosquitoAttackAIAction&)=delete;
        wxMosquitoAttackAIAction(wxMosquitoAttackAIAction&&)=delete;
        wxMosquitoAttackAIAction& operator=(wxMosquitoAttackAIAction&&)=delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,sparkplug::reconstruction::spCloneManager&) const override;
        wxMosquitoAttackAIActionResultForAnalysis ExitForAnalysis() noexcept;
        void vfunc_34_ClearForAnalysis() noexcept override; // PCslot2C, Exit5B4920.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxMosquitoAttackAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept{return state_;}
        wxMosquitoAttackAIActionResultForAnalysis LastResultForAnalysis() const noexcept{return lastResult_;}
    private:
        bool RefuseHookForAnalysis() noexcept;
        wxMosquitoAttackAIActionHost& host_;
        ProfileForAnalysis profile_;
        wxMosquitoAttackAIActionCapturedStateForAnalysis state_;
        wxMosquitoAttackAIActionResultForAnalysis lastResult_=wxMosquitoAttackAIActionResultForAnalysis::UnqualifiedProfile;
    };
}
