#include "wxTry2HoistState.h"
#include "Analysis/Host/wxTry2HoistStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxTry2HoistState>(); }
        const spRTTIRecord record{wxTry2HoistState::ClassID, wxCharacterState::ClassID,
            "wxTry2HoistState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxTry2HoistState::wxTry2HoistState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxTry2HoistState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxTry2HoistState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxTry2HoistState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxTry2HoistState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxTry2HoistState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xF0000884u) | 0x884u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        ClearOwnerActionControlFromState();
        return true;
    }
    void wxTry2HoistState::vfunc_2C(wxAnimationRequestForAnalysis&)
    {
        auto* host = dynamic_cast<wxTry2HoistStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxTry2HoistState requires a hoist-state host");
        ReleasePendingFromState();
        const bool nonzero = host->OwnerField218ForAnalysis(GetOwnerForAnalysis()) != 0;
        host->CallOwnerAfterReleaseForAnalysis(GetOwnerForAnalysis(), nonzero);
    }
    void wxTry2HoistState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxTry2HoistState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    std::uint32_t wxTry2HoistState::vfunc_38(std::uint32_t) const noexcept { return true; }
}
