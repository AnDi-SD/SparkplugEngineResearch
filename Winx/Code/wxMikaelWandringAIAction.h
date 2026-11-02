#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxMikaelWandringAIActionHost.h"

namespace sparkplug::reconstruction { class spNode; }

namespace winx::reconstruction
{
    class wxAICommandRecordForAnalysis;
    struct wxMikaelWandringAIActionCapturedStateForAnalysis final
    {
        // Logical binding for native base-prefix cell3A4. This is not an
        // appended native own member; its real assignment producer is open.
        wxAICommandRecordForAnalysis* borrowedBaseCommand3A4;
        std::array<std::uint8_t, 8> ownBytes3A8;
        sparkplug::reconstruction::spNode* borrowedSourceNode3B0;
        sparkplug::reconstruction::spNode* borrowedTargetNode3B4;
        std::array<std::uint8_t, 48> ownBytes3B8;
    };
    enum class wxMikaelWandringAIActionResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingTargetNode, MissingSourceNode,
        UnqualifiedGeometry, MissingCommand, CommandNotAdmitted, MissingOwner,
        MissingOwnerStorage, UnqualifiedNotify, UnqualifiedHook,
    };
    class wxMikaelWandringAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x12487A8E;
        enum class ProfileForAnalysis { Unqualified, PcCapturedFiniteEntryAndOwn48 };
        enum class Own4CProfileForAnalysis { Unqualified, PcCapturedFiniteStoredPoint };
        // Captured postconstruction data with explicit live borrowed inputs;
        // this does not reconstruct native allocation or owning lifetime.
        wxMikaelWandringAIAction(wxMikaelWandringAIActionHost&,
            const wxMikaelWandringAIActionCapturedStateForAnalysis&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxMikaelWandringAIAction() override = default;
        wxMikaelWandringAIAction(const wxMikaelWandringAIAction&) = delete;
        wxMikaelWandringAIAction& operator=(const wxMikaelWandringAIAction&) = delete;
        wxMikaelWandringAIAction(wxMikaelWandringAIAction&&) = delete;
        wxMikaelWandringAIAction& operator=(wxMikaelWandringAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        wxMikaelWandringAIActionResultForAnalysis EnterForAnalysis() noexcept;
        // Analytical name for actual PCselfslot48 body5C8F50. The body is
        // implemented below the Entry call, never supplied by a host callback.
        wxMikaelWandringAIActionResultForAnalysis OwnSlot48ForAnalysis() noexcept;
        // Independent per-call admission; the Entry/self48 profile does not
        // qualify this stored-point consumer or its unknown input producer.
        wxMikaelWandringAIActionResultForAnalysis OwnSlot4CForAnalysis(
            Own4CProfileForAnalysis = Own4CProfileForAnalysis::Unqualified) noexcept;
        void vfunc_30() noexcept override; // PCslot28 Entry5C8E10; argument unused.
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const wxMikaelWandringAIActionCapturedStateForAnalysis& CapturedStateForAnalysis() const noexcept { return state_; }
        wxMikaelWandringAIActionResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        bool IsProfileAdmittedForAnalysis() const noexcept;
        bool RefuseHookForAnalysis() noexcept;
        wxMikaelWandringAIActionHost& host_;
        const ProfileForAnalysis profile_;
        wxMikaelWandringAIActionCapturedStateForAnalysis state_;
        wxMikaelWandringAIActionResultForAnalysis lastResult_ = wxMikaelWandringAIActionResultForAnalysis::UnqualifiedProfile;
    };
}
