#include "wxIceGargoyleAIActionStateForAnalysis.h"
#include "wxAIAction.h"
#include "wxTargetManager.h"
#include "wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxPlanarAngleMath.h"
#include "Analysis/PC/wxIceGargoyleMath.h"
#include <bit>
#include <cmath>

namespace winx::reconstruction
{
    using Result=wxIceGargoyleResultForAnalysis;
    void wxIceGargoyleAIActionStateForAnalysis::CopyFromForAnalysis(
        const wxIceGargoyleAIActionStateForAnalysis& source) noexcept
    {
        cachedNode=source.cachedNode;phase=source.phase;stamp=source.stamp;word3A8=source.word3A8;
    }
    Result wxIceGargoyleAIActionStateForAnalysis::EnterForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        auto* manager=wxTargetManager::GetInstanceForAnalysis();
        if (!manager) return Result::UnqualifiedColdTargetManager;
        const auto identity=manager->CurrentWordsForAnalysis()[2];
        sparkplug::reconstruction::spNode* node=nullptr;
        if (identity)
        {
            const auto binding=host.ReadBorrowedRecordNode18ForAnalysis(identity);
            if (!binding) return Result::UnqualifiedTargetRecordData;
            node=*binding;
        }
        cachedNode=node;phase=1;stamp=0;
        if (!node)
        {
            host.SelectActionForAnalysis(action.GetOwnerForAnalysis(),0,0);
            return Result::SelectorRequested;
        }
        return Result::Completed;
    }
    void wxIceGargoyleAIActionStateForAnalysis::ExitForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        host.WriteCommandByteForAnalysis(host.ReadCommandForAnalysis(action.GetOwnerForAnalysis()),0x20,0);
        host.WriteCommandByteForAnalysis(host.ReadCommandForAnalysis(action.GetOwnerForAnalysis()),0x21,0);
        cachedNode=nullptr;
    }
    Result wxIceGargoyleAIActionStateForAnalysis::QueryForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host,
        wxPerceptionNumericProfileForAnalysis profile) noexcept
    {
        if (host.ReadPerceptionForAnalysis(action.GetOwnerForAnalysis()).FindNearestForAnalysis(profile)) return Result::Completed;
        host.SelectActionForAnalysis(action.GetOwnerForAnalysis(),0,0);
        return Result::SelectorRequested;
    }
    Result wxIceGargoyleAIActionStateForAnalysis::AimForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        if (!cachedNode) return Result::UnqualifiedCachedNode;
        auto* owner=action.GetOwnerForAnalysis();
        const auto origin=host.ReadOwnerNodeForAnalysis(owner).GetPositionForAnalysis();
        const auto target=cachedNode->GetWorldPositionForAnalysis();
        auto* command=host.ReadCommandForAnalysis(owner);
        float yaw;
        if (!analysis::pc::PcPlanarPositionYawForAnalysis(origin,target,yaw)) return Result::UnqualifiedNumericDomain;
        host.WriteCommandWordForAnalysis(command,8,std::bit_cast<std::uint32_t>(yaw));
        host.WriteCommandWordForAnalysis(host.ReadCommandForAnalysis(action.GetOwnerForAnalysis()),4,0x3C23D70A);
        return Result::Completed;
    }
    void wxIceGargoyleAIActionStateForAnalysis::Stage0ForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        auto* owner=action.GetOwnerForAnalysis();
        if (!stamp)
        {
            host.WriteCommandByteForAnalysis(host.ReadCommandForAnalysis(owner),0x20,1);
            stamp=host.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
            return;
        }
        if ((host.ReadCharacterFlags140ForAnalysis(owner)&0x7F80)==0x400)
        {
            host.WriteCommandByteForAnalysis(host.ReadCommandForAnalysis(owner),0x20,0);
            phase=1;
        }
    }
    Result wxIceGargoyleAIActionStateForAnalysis::WithdrawlStage1ForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        if (!cachedNode) return Result::UnqualifiedCachedNode;
        auto* owner=action.GetOwnerForAnalysis();
        const auto origin=host.ReadOwnerNodeForAnalysis(owner).GetPositionForAnalysis();
        const auto target=cachedNode->GetWorldPositionForAnalysis();
        bool close;
        if (!analysis::pc::PcIceGargoyleWithdrawlCloseForAnalysis(origin,target,close)) return Result::UnqualifiedNumericDomain;
        if (close) { phase=0;stamp=0; }
        host.WriteCommandWordForAnalysis(host.ReadCommandForAnalysis(owner),4,0x3F800000);
        auto* command=host.ReadCommandForAnalysis(action.GetOwnerForAnalysis());
        float yaw;
        if (!analysis::pc::PcPlanarPositionYawForAnalysis(origin,target,yaw)) return Result::UnqualifiedNumericDomain;
        host.WriteCommandWordForAnalysis(command,8,std::bit_cast<std::uint32_t>(yaw));
        return Result::Completed;
    }
    Result wxIceGargoyleAIActionStateForAnalysis::ClawStage1ForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        if (!cachedNode) return Result::UnqualifiedCachedNode;
        auto* owner=action.GetOwnerForAnalysis();
        const float ownerY=host.ReadOwnerNodeForAnalysis(owner).GetPositionForAnalysis()[1];
        const float targetY=cachedNode->GetWorldPositionForAnalysis()[1];
        bool withinBand,below;
        if (!analysis::pc::PcIceGargoyleClawHeightForAnalysis(ownerY,targetY,withinBand,below)) return Result::UnqualifiedNumericDomain;
        if (withinBand)
        {
            host.WriteCommandWordForAnalysis(host.ReadCommandForAnalysis(owner),4,0);
            stamp=0;phase=0;
            return Result::Completed;
        }
        host.WriteCommandWordForAnalysis(host.ReadCommandForAnalysis(owner),8,below?0:0x40490FDB);
        host.WriteCommandWordForAnalysis(host.ReadCommandForAnalysis(action.GetOwnerForAnalysis()),4,0x3F800000);
        return Result::Completed;
    }
    Result wxIceGargoyleAIActionStateForAnalysis::ClawBoundsForAnalysis(wxAIAction& action,wxIceGargoyleAIActionHost& host) noexcept
    {
        if (!cachedNode) return Result::UnqualifiedCachedNode;
        const float targetY=cachedNode->GetWorldPositionForAnalysis()[1];
        auto* owner=action.GetOwnerForAnalysis();
        const float lower=std::bit_cast<float>(host.ReadOwnerWordForAnalysis(owner,0x1CC));
        if (!std::isfinite(targetY) || !std::isfinite(lower)) return Result::UnqualifiedNumericDomain;
        if (targetY<lower)
        {
            // Native below-lower branch reloads action.owner20. The separate
            // above-upper branch below retains its earlier owner register.
            host.SelectActionForAnalysis(action.GetOwnerForAnalysis(),2,0);
            return Result::SelectorRequested;
        }
        const float upper=std::bit_cast<float>(host.ReadOwnerWordForAnalysis(owner,0x1B4));
        if (!std::isfinite(upper)) return Result::UnqualifiedNumericDomain;
        if (targetY<=upper) return Result::Completed;
        host.SelectActionForAnalysis(owner,2,0);
        return Result::SelectorRequested;
    }
}
