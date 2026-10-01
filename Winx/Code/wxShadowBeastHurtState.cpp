#include "wxShadowBeastHurtState.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateShadowBeastHurtState()
        { return std::make_unique<wxShadowBeastHurtState>(); }
        const spRTTIRecord record{wxShadowBeastHurtState::ClassID, wxCharacterState::ClassID,
            "wxShadowBeastHurtState", &wxCharacterState::StaticRTTI(), &CreateShadowBeastHurtState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxShadowBeastHurtState::wxShadowBeastHurtState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxShadowBeastHurtState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxShadowBeastHurtState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxShadowBeastHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxShadowBeastHurtState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxShadowBeastHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xF0008000u) | 0x8000u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        // The original queues over the old handle; there is no release here.
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxShadowBeastHurtState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxShadowBeastHurtState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
