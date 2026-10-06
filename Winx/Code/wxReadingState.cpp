#include "wxReadingState.h"
#include "Analysis/Host/wxReadingStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxReadingState>(); }
        const spRTTIRecord Record{wxReadingState::ClassID, wxCharacterState::ClassID,
            "wxReadingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxReadingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxReadingStateHost*>(&host);
            if (!typed) throw std::logic_error("wxReadingState requires a global manager/notification host");
            return *typed;
        }
    }
    wxReadingState::wxReadingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxReadingState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxReadingState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxReadingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxReadingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxReadingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC51AAE0 / PS22D28C0. Notification before caller key mutation.
        if (GetTransitionFlag1C())
        {
            Host(RequireHostForAnalysis()).SendReadingEntryForAnalysis(*this);
            request.packedKey = (request.packedKey & 0xf027ff80u) | 0x200000u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1C();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        return wxCharacterState::vfunc_1C(request);
    }
    bool wxReadingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC51AB90 / PS22D2660. First phase sends before pending release.
        if (GetTransitionFlag1E())
        {
            auto& host = Host(RequireHostForAnalysis());
            if (void* receiver = host.MainReceiverForAnalysis())
                host.SendReadingExitForAnalysis(receiver, *this, true);
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xf0478900u) | 0x400900u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        auto& host = Host(RequireHostForAnalysis());
        if (void* receiver = host.MainReceiverForAnalysis())
            host.SendReadingExitForAnalysis(receiver, *this, false);
        return wxCharacterState::vfunc_20(request);
    }
    void wxReadingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC51AA80 / PS22D2A30. Changed handles queue mode1 without release.
        request.packedKey = (request.packedKey & 0xf0078900u) | 0x900u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
    bool wxReadingState::vfunc_34(std::uint32_t)
    {
        // PC51AA60 / PS22D2B60: code, owner and pending are ignored.
        return Host(RequireHostForAnalysis()).GlobalWord1B0ForAnalysis() != 0x47;
    }
}
