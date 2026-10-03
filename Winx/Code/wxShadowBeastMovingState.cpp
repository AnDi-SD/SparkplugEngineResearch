#include "wxShadowBeastMovingState.h"
#include "Analysis/Host/wxShadowBeastMovingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxShadowBeastMovingState>(); }
        const spRTTIRecord record{wxShadowBeastMovingState::ClassID, wxCharacterState::ClassID,
            "wxShadowBeastMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxShadowBeastMovingState::wxShadowBeastMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxShadowBeastMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxShadowBeastMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxShadowBeastMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxShadowBeastMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxShadowBeastMovingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 523550 / PS2 3103D0: update, then base release/virtual update.
        vfunc_30(request);
        return wxCharacterState::vfunc_1C(request);
    }
    void wxShadowBeastMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC 523590 / PS2 3101D0. Preserve PC reads; PS2 keeps the first
        // scalar in F1 for its second comparison and omits the byte1D read.
        auto* host = dynamic_cast<wxShadowBeastMovingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxShadowBeastMovingState requires a shadow-moving host");
        if (host->OwnerMotionForAnalysis(GetOwnerForAnalysis()) < 0.2f)
        {
            request.packedKey &= 0xFFFFFF8Fu;
            const auto masked = request.packedKey & 0xF0FFFFFFu;
            (void)host->ReadOwnerControlByteForAnalysis(GetOwnerForAnalysis(), 0x1D);
            request.packedKey = masked | 0x800000u;
        }
        else if (host->OwnerMotionForAnalysis(GetOwnerForAnalysis()) < 0.5f)
            request.packedKey = (request.packedKey & 0xF07FFFDFu) | 0x50u;
        else
            request.packedKey = (request.packedKey & 0xF0FFFFDFu) | 0x800050u;
        request.packedKey &= 0xFF800070u;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
        // PS2 equivalence is limited to stable normal finite scalars and
        // signed zeros; special/denormal EE FPU values remain open.
    }
}
