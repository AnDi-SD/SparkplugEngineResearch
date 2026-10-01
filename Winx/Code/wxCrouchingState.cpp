#include "wxCrouchingState.h"
#include "Analysis/Host/wxCrouchingStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCrouchingState()
        { return std::make_unique<wxCrouchingState>(); }
        const spRTTIRecord record{wxCrouchingState::ClassID, wxCharacterState::ClassID,
            "wxCrouchingState", &wxCharacterState::StaticRTTI(), &CreateCrouchingState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxCrouchingState::wxCrouchingState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxCrouchingState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxCrouchingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxCrouchingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxCrouchingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxCrouchingState::StartTransitionForAnalysis(wxAnimationRequestForAnalysis& request,
        const std::uint32_t mask, const std::uint32_t bits)
    {
        request.packedKey = (request.packedKey & mask) | bits;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        ReleasePendingFromState();
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
    }
    bool wxCrouchingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1C())
        {
            StartTransitionForAnalysis(request, 0xF027800Fu, 0x00200000u);
            auto* host = dynamic_cast<wxCrouchingStateHost*>(&RequireHostForAnalysis());
            if (!host) throw std::logic_error("wxCrouchingState requires a crouching-state host");
            host->SetEntryServiceByteForAnalysis(0);
            ClearTransitionFlag1C();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_1C(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    bool wxCrouchingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1E())
        {
            StartTransitionForAnalysis(request, 0xF047800Fu, 0x00400000u);
            ClearTransitionFlag1E();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_20(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    std::uint32_t wxCrouchingState::ComposeCrouchingKeyForAnalysis(
        std::uint32_t input, const float motion) noexcept
    {
        if ((input & 0x00007F80u) == 0x00000200u) input &= 0xFFFF807Fu;
        input = motion < 0.2f ? (input & 0xFFFFFF8Fu)
            : ((input & 0xFFFFFFDFu) | 0x50u);
        return input & 0xF007FFFFu;
    }
    void wxCrouchingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxCrouchingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxCrouchingState requires a crouching-state host");
        request.packedKey = ComposeCrouchingKeyForAnalysis(request.packedKey,
            host->CrouchingMotionForAnalysis(GetOwnerForAnalysis()));
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
    bool wxCrouchingState::vfunc_34(const std::uint32_t target)
    { return target != 1 && target != 3 && target != 4 && target != 5 && target != 8; }
}
