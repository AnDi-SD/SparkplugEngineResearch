#include "wxMikaelWaitingState.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMikaelWaitingState>(); }
        const spRTTIRecord record{wxMikaelWaitingState::ClassID, wxCharacterState::ClassID,
            "wxMikaelWaitingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMikaelWaitingState::wxMikaelWaitingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMikaelWaitingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMikaelWaitingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMikaelWaitingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMikaelWaitingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMikaelWaitingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 5A8190 / PS2 314A80: update first, clear control, then the
        // base entry releases pending and performs a SECOND virtual update.
        vfunc_30(request);
        ClearOwnerActionControlFromState();
        return wxCharacterState::vfunc_1C(request);
    }
    void wxMikaelWaitingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC 5A82A0; the upper nibble survives, subfield23 is forced to1.
        request.packedKey = (request.packedKey & 0xF0800000u) | 0x800000u;
        auto& host = RequireHostForAnalysis();
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
