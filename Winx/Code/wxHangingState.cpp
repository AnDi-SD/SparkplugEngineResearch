#include "wxHangingState.h"
#include "Analysis/Host/wxHangingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxHangingState>(); }
        const spRTTIRecord record{wxHangingState::ClassID, wxCharacterState::ClassID,
            "wxHangingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxHangingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxHangingStateHost*>(&host);
            if (!result) throw std::logic_error("Hanging requires an actor/node host");
            return *result;
        }
        constexpr float low = 0.78539818525314331055f;
        double High(wxHangingStateHost& host)
        { return host.MovementNumericProfileForAnalysis() == wxCharacterMovementNumericProfileForAnalysis::PS2Finite
            ? double(2.35619449615478515625f) : double(low) * 3.0; }
        void Direction(wxAnimationRequestForAnalysis& request, std::uint32_t value)
        { request.packedKey = (request.packedKey & 0xFFFFFF8Fu) | value; }
    }
    wxHangingState::wxHangingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxHangingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxHangingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxHangingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxHangingState>(); manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxHangingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        ++field3C_;
        auto& host = Host(RequireHostForAnalysis());
        // PC keeps the captured angle on x87 while checking motion.
        const float angle = host.UpdateAngleForAnalysis(GetOwnerForAnalysis());
        const float motion = host.UpdateMotionForAnalysis(GetOwnerForAnalysis());
        if (motion < 0.2f) Direction(request, 0);
        else if (angle < low)
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.3f);
            Direction(request, 0x40);
        }
        else if (double(angle) > High(host))
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.3f);
            Direction(request, 0x30);
        }
        else { Direction(request, 0); ClearOwnerActionControlFromState(); }
        request.packedKey &= 0xF000007Fu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState(); QueuePendingFromState(handle, true, true); SetPendingHandleFromState(handle);
    }
    bool wxHangingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1C())
        {
            host.ClearGameManagerWord504ForAnalysis(); field3C_ = 0;
            request.packedKey = (request.packedKey & 0xFFA0000Fu) | 0x200000u;
            const auto kind = host.OwnerKindForAnalysis(GetOwnerForAnalysis());
            if (kind == 1 || kind == 17 || kind == 18)
            {
                const auto pair = host.EntryComparePairForAnalysis(GetOwnerForAnalysis());
                request.packedKey = pair && (*pair)[1] > (*pair)[0]
                    ? (request.packedKey & 0xF0FFFFFFu) | 0x800000u
                    : (request.packedKey & 0xF17FFFFFu) | 0x1000000u;
            }
            else
            {
                host.RotateHangingNodeForAnalysis(host.OwnerMovementTargetForAnalysis(GetOwnerForAnalysis()), 3.14159274101257324219f);
                request.packedKey &= 0xF07FFFFFu;
            }
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1C();
            return false;
        }
        if (!host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.25f);
        return wxCharacterState::vfunc_1C(request);
    }
    bool wxHangingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1E())
        {
            host.ClearGameManagerWord504ForAnalysis(); ReleasePendingFromState(); PrepareMovementFromState(false);
            host.WriteEntityFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 1);
            request.packedKey = (request.packedKey & 0xFFD8000Fu) | 0x400000u;
            const auto word = host.ExitPackedWordForAnalysis(GetOwnerForAnalysis());
            if ((word & 15u) == 5)
            {
                const float angle = host.ExitControlAngleForAnalysis(GetOwnerForAnalysis());
                request.packedKey = angle >= low && double(angle) <= High(host)
                    ? (request.packedKey & 0xF1FFFFFFu) | 0x1800000u
                    : (request.packedKey & 0xF17FFFFFu) | 0x1000000u;
            }
            else
            {
                const bool eligible = (word & 15u) == 0 && (word & 0x180000u) != 0x100000u
                    && field3C_ != 0 && host.ExitEligibilityByteForAnalysis(GetOwnerForAnalysis())
                    && host.ExitEntityAngleForAnalysis(GetOwnerForAnalysis()) < low;
                request.packedKey = eligible ? request.packedKey & 0xF07FFFFFu
                    : (request.packedKey & 0xF0FFFFFFu) | 0x800000u;
                host.WriteDirectControlByte1BForAnalysis(GetOwnerForAnalysis(), eligible ? 0 : 1);
            }
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1E();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            host.WriteEntityFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 0);
            void* const node = host.OwnerMovementTargetForAnalysis(GetOwnerForAnalysis());
            wxHangingStateHost::Vector3 position{};
            for (unsigned i = 0; i < 3; ++i) position[i] = host.ReadNodePositionWordForAnalysis(node, 0x20u + i * 4u);
            host.SetFinalControlPositionForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), position);
            PrepareMovementFromState(false); ClearOwnerActionControlFromState();
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            return wxCharacterState::vfunc_20(request);
        }
        FinishMovementFromState(false); return false;
    }
}
