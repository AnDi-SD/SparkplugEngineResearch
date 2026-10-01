#include "wxFrogHurtState.h"
#include "Analysis/Host/wxCharacterControlStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFrogHurtState()
        { return std::make_unique<wxFrogHurtState>(); }
        const spRTTIRecord record{wxFrogHurtState::ClassID, wxCharacterState::ClassID,
            "wxFrogHurtState", &wxCharacterState::StaticRTTI(), &CreateFrogHurtState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFrogHurtState::wxFrogHurtState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFrogHurtState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxFrogHurtState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFrogHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFrogHurtState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxFrogHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxCharacterControlStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxFrogHurtState requires a control-state host");
        // PC520C1D / PS22F4284 clears byte1A before touching the request.
        host->ClearOwnerControlByteForAnalysis(GetOwnerForAnalysis(), 0x1A);
        request.packedKey = (request.packedKey & 0xF0008000u) | 0x8000u;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxFrogHurtState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxFrogHurtState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
