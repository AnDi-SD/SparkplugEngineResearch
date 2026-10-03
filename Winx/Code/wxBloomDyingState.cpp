#include "wxBloomDyingState.h"
#include "Analysis/Host/wxDyingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxBloomDyingState>(); }
        const spRTTIRecord record{wxBloomDyingState::ClassID, wxCharacterState::ClassID,
            "wxBloomDyingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxBloomDyingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxBloomDyingStateHost*>(&host);
            if (!result) throw std::logic_error("wxBloomDyingState requires a Bloom dying-state host");
            return *result;
        }
    }
    wxBloomDyingState::wxBloomDyingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxBloomDyingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxBloomDyingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxBloomDyingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBloomDyingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxBloomDyingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5134B0 / PS22C8370. Manager, speed, byte reset, then packed key.
        auto& host = Host(RequireHostForAnalysis());
        host.BloomDyingManager593B00ForAnalysis(7000, 0);
        host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
        sentDyingOver_ = 0;
        const std::uint32_t low = request.packedKey & 0xFu;
        if (low != 1 && low != 3) request.packedKey &= 0xFFFFFFF0u;
        request.packedKey = (request.packedKey & 0xFFF9800Fu) | 0x18000u;
        if (host.BloomDyingOwnerFlagForAnalysis(GetOwnerForAnalysis()) != 0)
            request.packedKey = (request.packedKey & 0xFF97FFF0u) | 0x100000u;
        else
            request.packedKey &= 0xFF87FFFFu;
        request.packedKey &= 0xF07FFFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxBloomDyingState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        // PC513590 / PS22C8210. Completion is consumed before the once guard.
        auto& host = Host(RequireHostForAnalysis());
        if (host.BloomDyingUpdateGateForAnalysis(GetOwnerForAnalysis()) != 0) return;
        host.ResetDyingControlForAnalysis(GetOwnerForAnalysis());
        if (!host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(),
            GetPendingHandleForAnalysis(), true)) return;
        if (sentDyingOver_ != 0) return;
        host.BloomDyingManager593A60ForAnalysis(1000, 0);
        host.BloomDyingManager595450ForAnalysis(0x35, 1);
        host.BloomDyingManager5953E0ForAnalysis(0x40, 0);
        if (void* target = host.DyingNotificationTargetForAnalysis(GetOwnerForAnalysis()))
            host.SendDyingNotificationForAnalysis(target, *this, 0x27E9, 0, 0);
        sentDyingOver_ = 1;
    }
}
