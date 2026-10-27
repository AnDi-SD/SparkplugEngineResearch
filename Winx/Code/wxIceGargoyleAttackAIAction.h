#pragma once
#include "Code/wxAIAction.h"
#include "Code/wxPerception.h"
#include "Analysis/Host/wxIceGargoyleAttackAIActionHost.h"
#include <optional>

namespace winx::reconstruction
{
    enum class wxIceGargoyleAttackProfileForAnalysis { HooksOnly,PcFiniteGeometry };
    enum class wxIceGargoyleAttackResultForAnalysis
    {
        Completed,SelectorRequested,UnqualifiedUpdateProfile,UnqualifiedColdTargetManager,
        UnqualifiedTargetRecordData,UnqualifiedCachedNode,UnqualifiedNumericDomain,
        UnqualifiedNodeWorld,UnqualifiedSpeed,UnqualifiedBrainDelivery
    };
    struct wxIceGargoyleAttackStateForAnalysis final
    {
        std::array<std::uint32_t,3> waypoint{}; //PC3A8..3B0
        std::uint8_t toggle=0; //3B4
        std::array<std::uint32_t,3> direction{}; //3B8..3C0
        // Original constructor leaves3C4 unwritten. A guessed zero would
        // erase a real precondition of movement and the own Copy omission.
        std::optional<std::uint32_t> speed;
        std::uint32_t word3C8=0,word3CC=0;
        std::uint8_t byte3D0=1,byte3D1=1,byte3D2=0;
        std::uint32_t threshold=0x3F000000;
        sparkplug::reconstruction::spNode* cachedNode=nullptr; //borrowed3D8
        std::uint32_t phase=0,time=0; //3DC/3E0
        std::uint8_t latch=0; //3E4
    };
    class wxIceGargoyleAttackAIAction final:public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x1B4441B7;
        explicit wxIceGargoyleAttackAIAction(wxIceGargoyleAttackAIActionHost&,
            wxIceGargoyleAttackProfileForAnalysis=wxIceGargoyleAttackProfileForAnalysis::HooksOnly,
            wxPerceptionNumericProfileForAnalysis=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
        ~wxIceGargoyleAttackAIAction() override=default;
        static void SetFactoryHostForAnalysis(wxIceGargoyleAttackAIActionHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        // Other two bool updates retain the actual common literal true body.
        void vfunc_30() noexcept override { (void)EnterForAnalysis(); }
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override { (void)QueryForAnalysis(); }
        wxIceGargoyleAttackResultForAnalysis EnterForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis QueryForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis UpdateForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis NotifyForAnalysis(const void*) noexcept;
        wxIceGargoyleAttackResultForAnalysis CheckHeightForAnalysis() noexcept;
        wxIceGargoyleAttackStateForAnalysis& StateForAnalysis() noexcept { return state_; }
        const wxIceGargoyleAttackStateForAnalysis& StateForAnalysis() const noexcept { return state_; }
        wxIceGargoyleAttackResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        wxIceGargoyleAttackResultForAnalysis RefreshTargetForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis OrientForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis Stage0ForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis Stage1ForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis Stage2ForAnalysis() noexcept;
        wxIceGargoyleAttackResultForAnalysis Stage3ForAnalysis() noexcept;
        wxIceGargoyleAttackAIActionHost& host_;
        wxIceGargoyleAttackProfileForAnalysis profile_;
        wxPerceptionNumericProfileForAnalysis perceptionProfile_;
        wxIceGargoyleAttackStateForAnalysis state_;
        wxIceGargoyleAttackResultForAnalysis lastResult_=wxIceGargoyleAttackResultForAnalysis::Completed;
    };
}
