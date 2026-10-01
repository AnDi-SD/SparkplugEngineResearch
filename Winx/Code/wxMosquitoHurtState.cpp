#include "wxMosquitoHurtState.h"
#include "Analysis/Host/wxCharacterSpeedStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMosquitoHurtState>(); }
        const spRTTIRecord record{wxMosquitoHurtState::ClassID, wxCharacterState::ClassID,
            "wxMosquitoHurtState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMosquitoHurtState::wxMosquitoHurtState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMosquitoHurtState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMosquitoHurtState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMosquitoHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMosquitoHurtState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMosquitoHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxCharacterSpeedStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxMosquitoHurtState requires a speed host");
        request.packedKey = (request.packedKey & 0xFF808001u) | 0x8001u;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        host->SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 2.0f);
        return true;
    }
    bool wxMosquitoHurtState::vfunc_20(wxAnimationRequestForAnalysis&)
    {
        auto* host = dynamic_cast<wxCharacterSpeedStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxMosquitoHurtState requires a speed host");
        host->SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
        return true;
    }
    bool wxMosquitoHurtState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
