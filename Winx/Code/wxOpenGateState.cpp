#include "wxOpenGateState.h"
#include "Analysis/Host/wxOpenGateStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxOpenGateState>(); }
        const spRTTIRecord record{wxOpenGateState::ClassID, wxCharacterState::ClassID,
            "wxOpenGateState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxOpenGateState::wxOpenGateState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxOpenGateState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxOpenGateState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxOpenGateState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxOpenGateState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxOpenGateState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5A7800 / PS22D03C0: queue mode0, without releasing old pending.
        request.packedKey = (request.packedKey & 0xFF878A80u) | 0xA80u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
        }
        ClearOwnerActionControlFromState();
    }
    bool wxOpenGateState::vfunc_34(std::uint32_t)
    {
        // PC5A77C0 / PS22D04E0: no once guard; null is queried too.
        const bool complete = RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
        if (complete)
        {
            auto* host = dynamic_cast<wxOpenGateStateHost*>(&RequireHostForAnalysis());
            if (!host) throw std::logic_error("wxOpenGateState requires a notification host");
            host->SendFilteredNotificationForAnalysis(*this, 0x27DE, 0x12, nullptr, 0);
        }
        return complete;
    }
    void wxOpenGateState::vfunc_3C(const void* event)
    {
        // PC5A7770 / PS22D0550, complete case-sensitive event_spin match.
        auto* host = dynamic_cast<wxOpenGateStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxOpenGateState requires an event host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxOpenGateState host returned a null event tag name");
        if (std::strcmp(name, "event_spin") != 0) return;
        void* const entity = host->OwnerField124ForAnalysis(GetOwnerForAnalysis());
        host->SendFilteredNotificationForAnalysis(*this, 0x2731, 6, entity, 0);
    }
}
