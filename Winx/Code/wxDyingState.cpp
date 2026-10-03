#include "wxDyingState.h"
#include "Analysis/Host/wxDyingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDyingState>(); }
        const spRTTIRecord record{wxDyingState::ClassID, wxCharacterState::ClassID,
            "wxDyingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxDyingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxDyingStateHost*>(&host);
            if (!result) throw std::logic_error("wxDyingState requires a dying-state host");
            return *result;
        }
    }
    wxDyingState::wxDyingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDyingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDyingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDyingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDyingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDyingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC518DF0 / PS22CB7B0. No release of the previous pending handle.
        auto& host = Host(RequireHostForAnalysis());
        sentDyingOver_ = 0;
        host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
        request.packedKey = (request.packedKey & 0xFF818000u) | 0x18000u;
        if (host.DyingOwnerKindForAnalysis(GetOwnerForAnalysis()) != 24
            || host.DyingOwnerFlagForAnalysis(GetOwnerForAnalysis()) != 0)
            request.packedKey &= 0xF07FFFFFu;
        else
            request.packedKey = (request.packedKey & 0xF0FFFFFFu) | 0x800000u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxDyingState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        // PC518EA0 / PS22CB660. Control reset always precedes the once guard.
        auto& host = Host(RequireHostForAnalysis());
        host.ResetDyingControlForAnalysis(GetOwnerForAnalysis());
        if (sentDyingOver_ != 0) return;
        if (!host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(),
            GetPendingHandleForAnalysis(), true)) return;
        if (void* target = host.DyingNotificationTargetForAnalysis(GetOwnerForAnalysis()))
            host.SendDyingNotificationForAnalysis(target, *this, 0x27D2, 0x13, 0);
        if (void* target = host.DyingNotificationTargetForAnalysis(GetOwnerForAnalysis()))
            host.SendDyingNotificationForAnalysis(target, *this, 0x2729, 0, 0);
        sentDyingOver_ = 1;
        host.DyingDiagnosticForAnalysis(
            "Dying State: m_bSentDyingOverMsg = true for : 0x%x", *this);
    }
}
