#pragma once
#include "Code/wxAIAction.h"
#include "Code/wxPerception.h"
#include "Analysis/Host/wxFlyingWanderAIActionHost.h"

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    enum class wxFlyingWanderQueryResultForAnalysis { PermissionDisabled,NoTarget,SelectorRequested };
    class wxFlyingWanderAIAction final:public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x7529750D;
        using OwnWordsForAnalysis=std::array<std::uint32_t,12>; // PC3A8..3D4
        explicit wxFlyingWanderAIAction(wxFlyingWanderAIActionHost&,
            wxPerceptionNumericProfileForAnalysis=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
        ~wxFlyingWanderAIAction() override;
        wxFlyingWanderAIAction(const wxFlyingWanderAIAction&)=delete;
        wxFlyingWanderAIAction& operator=(const wxFlyingWanderAIAction&)=delete;
        static void SetFactoryHostForAnalysis(wxFlyingWanderAIActionHost*,
            wxPerceptionNumericProfileForAnalysis=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override {} // native no-op, no forwarding
        // All three bool update slots use the actual common literal true body.
        // Inherited wxAIAction implementations preserve that behavior.
        void vfunc_30() noexcept override { ownWords_[4]=ownWords_[5]=0; }
        void vfunc_34_ClearForAnalysis() noexcept override {} // literal no-op Exit
        void vfunc_38() noexcept override;
        [[nodiscard]] wxFlyingWanderQueryResultForAnalysis QueryForAnalysis() noexcept;
        const OwnWordsForAnalysis& GetOwnWordsForAnalysis() const noexcept { return ownWords_; }
        void SetOwnWordsForAnalysis(const OwnWordsForAnalysis& words) noexcept { ownWords_=words; }
        std::uint8_t GetField3D8ForAnalysis() const noexcept { return field3D8_; }
        void SetField3D8ForAnalysis(std::uint8_t value) noexcept { field3D8_=value; }
        // Our shared-ownership representation of the native own16-bit ref and
        // root reference. A returned handle explicitly retains this Node.
        std::shared_ptr<sparkplug::reconstruction::spNode> HelperNodeForAnalysis() const noexcept { return helperNode_; }
    private:
        wxFlyingWanderAIActionHost& host_;
        wxPerceptionNumericProfileForAnalysis numericProfile_;
        OwnWordsForAnalysis ownWords_{};
        std::uint8_t field3D8_=0;
        std::shared_ptr<sparkplug::reconstruction::spNode> helperNode_;
    };
}
