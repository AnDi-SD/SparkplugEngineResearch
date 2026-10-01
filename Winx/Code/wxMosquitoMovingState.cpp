#include "wxMosquitoMovingState.h"
#include "Analysis/Host/wxMosquitoMovingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMosquitoMovingState()
        { return std::make_unique<wxMosquitoMovingState>(); }
        const spRTTIRecord record{wxMosquitoMovingState::ClassID, wxCharacterState::ClassID,
            "wxMosquitoMovingState", &wxCharacterState::StaticRTTI(), &CreateMosquitoMovingState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& wxMosquitoMovingState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxMosquitoMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMosquitoMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMosquitoMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxMosquitoMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xFFFFFFF1u) | 1u;
        auto* host = dynamic_cast<wxMosquitoMovingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxMosquitoMovingState requires a mosquito-moving host");
        if (host->OwnerMotionForAnalysis(GetOwnerForAnalysis()) < 0.005f)
        {
            request.packedKey &= 0xFFFFFF8Fu;
            ClearOwnerActionControlFromState();
        }
        else
        {
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        }
        request.packedKey &= 0xF007FFFFu;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        // PC521350 / PS22F5598: no release and no completion reset (mode1).
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
