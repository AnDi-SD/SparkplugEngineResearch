#pragma once
#include "Analysis/Host/wxIceGargoyleAIActionHost.h"
#include "Code/wxPerception.h"

namespace winx::reconstruction
{
    class wxAIAction;
    enum class wxIceGargoyleUpdateProfileForAnalysis { HooksOnly,PcFiniteGeometry };
    enum class wxIceGargoyleResultForAnalysis
    {
        Completed,SelectorRequested,UnqualifiedUpdateProfile,UnqualifiedCachedNode,
        UnqualifiedNumericDomain,UnqualifiedColdTargetManager,UnqualifiedTargetRecordData
    };
    // Our composition for identical own fields/bodies in two physical
    // wxAIAction siblings. This is NOT a recovered native base class/layout.
    struct wxIceGargoyleAIActionStateForAnalysis final
    {
        std::uint32_t word3A8=0x3F000000; //0.5f; original semantic role unknown
        sparkplug::reconstruction::spNode* cachedNode=nullptr; //borrowed PC3AC
        std::uint32_t phase=1; //PC3B0
        std::uint32_t stamp=0; //PC3B4
        void CopyFromForAnalysis(const wxIceGargoyleAIActionStateForAnalysis&) noexcept;
        wxIceGargoyleResultForAnalysis EnterForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        void ExitForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        wxIceGargoyleResultForAnalysis QueryForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&,
            wxPerceptionNumericProfileForAnalysis) noexcept;
        wxIceGargoyleResultForAnalysis AimForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        void Stage0ForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        wxIceGargoyleResultForAnalysis WithdrawlStage1ForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        wxIceGargoyleResultForAnalysis ClawStage1ForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
        wxIceGargoyleResultForAnalysis ClawBoundsForAnalysis(wxAIAction&,wxIceGargoyleAIActionHost&) noexcept;
    };
}
