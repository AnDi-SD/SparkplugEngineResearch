#include "wxDefendingState.h"
#include "Analysis/Host/wxDefendingStateHost.h"
#include "Analysis/PC/wxDefendingNumeric.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDefendingState>(); }
        const spRTTIRecord record{wxDefendingState::ClassID, wxCharacterState::ClassID,
            "wxDefendingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxDefendingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxDefendingStateHost*>(&host);
            if (!result) throw std::logic_error("Defending requires a node/service host");
            return *result;
        }
        void Mark(wxDefendingStateHost& host, void* node)
        { host.WriteDefendingNodeFlagsForAnalysis(node, host.ReadDefendingNodeFlagsForAnalysis(node) | 1u); }
    }
    wxDefendingState::wxDefendingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDefendingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDefendingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDefendingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDefendingState>(); manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxDefendingState::InitializeNodesForAnalysis()
    {
        auto& host = Host(RequireHostForAnalysis());
        field3C_ = host.FindDefendingNodeForAnalysis(host.OwnerNodeSearchRootForAnalysis(GetOwnerForAnalysis()), "shield_master", true, false);
        if (!field3C_) return; // Preserve existing child fields when master lookup fails.
        field40_ = host.FindDefendingNodeForAnalysis(field3C_, "shield_end", true, false);
        field44_ = host.FindDefendingNodeForAnalysis(field3C_, "shield_hit", true, false);
        SetNodesEnabledForAnalysis(false, true);
    }
    void wxDefendingState::SetNodesEnabledForAnalysis(bool enabled, bool silent)
    {
        if (!field3C_) return;
        auto& host = Host(RequireHostForAnalysis());
        if (!enabled)
        {
            if (!silent)
            {
                host.SendDefendingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27D2, 7, 0);
                host.SendDefendingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27D1, 8, 0);
            }
            host.InvokeDefendingNodeForAnalysis(field3C_, false, true);
            host.InvokeDefendingParticleForAnalysis(field3C_, "ptc");
            return;
        }
        host.InvokeDefendingNodeForAnalysis(field3C_, true, true);
        host.InvokeDefendingNodeForAnalysis(field40_, false, false);
        host.InvokeDefendingNodeForAnalysis(field44_, false, false);
        const auto word = host.OwnerDefendingPackedWordForAnalysis(GetOwnerForAnalysis()) & 15u;
        void* const source = word == 0 || word == 2 ? nullptr
            : host.FindDefendingNodeForAnalysis(host.OwnerNodeSearchRootForAnalysis(GetOwnerForAnalysis()), "SubMaster", true, false);
        if (source)
        {
            // PC captures all three values before writing X,Z,Y; PS2 X,Y,Z.
            const auto x = host.ReadDefendingNodeWordForAnalysis(source, 0x20);
            const auto y = host.ReadDefendingNodeWordForAnalysis(source, 0x24);
            const auto z = host.ReadDefendingNodeWordForAnalysis(source, 0x28);
            host.WriteDefendingNodeWordForAnalysis(field3C_, 0x20, x);
            if (host.NumericProfileForAnalysis() == wxDefendingNumericProfileForAnalysis::PCNearest64)
            {
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x28, z);
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x24, y);
            }
            else
            {
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x24, y);
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x28, z);
            }
        }
        else
        {
            const std::uint32_t z = word == 0 || word == 2 ? 0xC2200000u : 0u;
            if (host.NumericProfileForAnalysis() == wxDefendingNumericProfileForAnalysis::PCNearest64)
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x28, z);
            host.WriteDefendingNodeWordForAnalysis(field3C_, 0x20, 0);
            host.WriteDefendingNodeWordForAnalysis(field3C_, 0x24, 0x42960000u);
            if (host.NumericProfileForAnalysis() == wxDefendingNumericProfileForAnalysis::PS2Finite)
                host.WriteDefendingNodeWordForAnalysis(field3C_, 0x28, z);
        }
        Mark(host, field3C_);
        host.SendDefendingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27D1, 7, 0);
    }
    void wxDefendingState::AdvanceExitNodesForAnalysis()
    {
        if (!field3C_) return;
        auto& host = Host(RequireHostForAnalysis());
        if (field48_ == 0.0f) host.WriteSharedDefendingStageForAnalysis(0);
        // PC captures the stage before asking the clock singleton for delta;
        // PS2 reads it after the rounded scalar comparison.
        const bool pc = host.NumericProfileForAnalysis() == wxDefendingNumericProfileForAnalysis::PCNearest64;
        auto stage = pc ? host.ReadSharedDefendingStageForAnalysis() : 0u;
        const float previous = field48_, delta = host.ReadDefendingDeltaForAnalysis();
        field48_ = pc ? static_cast<float>(double(previous) + double(delta)) : previous + delta;
        if (pc ? winx::evidence::pc::DefendingSumBelowThresholdForAnalysis(previous, delta)
               : field48_ < 0.17000000178813934326f) return;
        if (!pc) stage = host.ReadSharedDefendingStageForAnalysis();
        if (stage == 0)
        {
            host.InvokeDefendingNodeForAnalysis(field40_, true, true);
            for (auto offset : {0x30u, 0x34u, 0x38u}) host.WriteDefendingNodeWordForAnalysis(field40_, offset, 0x3F800000u);
            if (pc)
            {
                Mark(host, field40_);
                host.WriteSharedDefendingStageForAnalysis(host.ReadSharedDefendingStageForAnalysis() + 1u);
            }
            else
            {
                host.WriteSharedDefendingStageForAnalysis(host.ReadSharedDefendingStageForAnalysis() + 1u);
                Mark(host, field40_);
            }
        }
        else if (stage == 1)
        {
            const bool bit = (host.ReadDefendingNodeFlagsForAnalysis(field40_) & 0x200u) != 0;
            host.InvokeDefendingNodeForAnalysis(field40_, !bit, true);
            if (!bit) host.WriteSharedDefendingStageForAnalysis(host.ReadSharedDefendingStageForAnalysis() + 1u);
        }
        else if (stage == 2 && (host.ReadDefendingNodeFlagsForAnalysis(field3C_) & 0x200u))
            SetNodesEnabledForAnalysis(false, true);
    }
    void wxDefendingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        void* const control = host.DirectDefendingControlForAnalysis(GetOwnerForAnalysis());
        request.packedKey &= 0xF007FF8Fu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            if (!field50_ || host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            {
                const bool old = field50_ != 0;
                field50_ = (request.packedKey & 0x78000u) == 0x8000u ? 1 : 0;
                if (!old || !field50_) ReleasePendingFromState(true);
                QueuePendingFromState(handle, field50_ == 0, true);
                SetPendingHandleFromState(handle);
            }
        }
        if (field44_)
        {
            if (host.ReadDefendingHitByteForAnalysis(control))
            {
                host.SendDefendingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27D1, 23, 0);
                host.InvokeDefendingNodeForAnalysis(field44_, true, true);
                host.SendDefendingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x2719, 0, 0);
                host.WriteDefendingHitByteForAnalysis(control, 0);
                field4C_ = host.ReadDefendingClockForAnalysis() + 300u;
                host.InvokeDefendingScalarServiceForAnalysis(0, 1.0f, 1.0f, 0.5f);
            }
            else if (field4C_ && field4C_ < host.ReadDefendingClockForAnalysis())
                host.InvokeDefendingNodeForAnalysis(field44_, false, true);
        }
        ClearOwnerActionControlFromState();
    }
    bool wxDefendingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1C())
        {
            SetNodesEnabledForAnalysis(true, false); field50_ = 0; field4C_ = 0;
            request.packedKey = (request.packedKey & 0xF0207F8Fu) | 0x200000u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 2.0f);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1C();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            return wxCharacterState::vfunc_1C(request);
        }
        ClearOwnerActionControlFromState(); return false;
    }
    bool wxDefendingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1E())
        {
            ReleasePendingFromState(); field48_ = 0;
            request.packedKey = (request.packedKey & 0xF040018Fu) | 0x400180u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 2.0f);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1E();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            SetNodesEnabledForAnalysis(false, false);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            return wxCharacterState::vfunc_20(request);
        }
        AdvanceExitNodesForAnalysis(); ClearOwnerActionControlFromState(); return false;
    }
}
