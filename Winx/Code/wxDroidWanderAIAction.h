#pragma once
#include "wxAIAction.h"
#include "Analysis/Host/wxDroidWanderAIActionHost.h"

namespace winx::reconstruction
{
    enum class wxDroidWanderResultForAnalysis
    {
        Completed,UnqualifiedProfile,MissingOwner,MissingCommand,
        UnqualifiedNotify,UnqualifiedTick,UnqualifiedEntry,UnqualifiedQuery
    };

    class wxDroidWanderAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x34154F9E;
        // Explicit post-construction binding. It does not reproduce the whole
        // native factory/base initialization, Copy or Clone. The actual PC
        // factory uses exactly the AIAction3A8 prefix; no derived fields are
        // invented from the adjacent constructor of another class.
        wxDroidWanderAIAction(wxDroidWanderAIActionHost&,void* capturedOwner) noexcept;
        ~wxDroidWanderAIAction() override=default;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        // Portable PS2-offset name24 is native PCslot1C (5BCE40), not Entry.
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        wxDroidWanderResultForAnalysis PublishCommandForAnalysis() noexcept;
        wxDroidWanderResultForAnalysis ClearCommandForAnalysis() noexcept;
        wxDroidWanderResultForAnalysis LastResultForAnalysis() const noexcept{return lastResult_;}
    private:
        bool AdmitForAnalysis() noexcept;
        wxDroidWanderAIActionHost& host_;
        // Analytical admission status; no corresponding native field.
        wxDroidWanderResultForAnalysis lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedProfile;
    };
}
