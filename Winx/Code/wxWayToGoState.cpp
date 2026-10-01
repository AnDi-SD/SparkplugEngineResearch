#include "wxWayToGoState.h"
#include "Analysis/Host/wxCharacterControlStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateWayToGoState()
        { return std::make_unique<wxWayToGoState>(); }
        const spRTTIRecord record{wxWayToGoState::ClassID, wxCharacterState::ClassID,
            "wxWayToGoState", &wxCharacterState::StaticRTTI(), &CreateWayToGoState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    const spRTTIRecord& wxWayToGoState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxWayToGoState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxWayToGoState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxWayToGoState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxWayToGoState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 5A6870 / PS2 2D5570: queue without releasing the old handle,
        // store the new one only after playback, then clear control byte/word.
        request.packedKey &= 0xF0007F80u;
        auto* host = dynamic_cast<wxCharacterControlStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxWayToGoState requires a control-state host");
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        host->ClearOwnerControlByteForAnalysis(GetOwnerForAnalysis(), 0x61);
        ClearOwnerActionControlFromState();
        return true;
    }
    void wxWayToGoState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxWayToGoState::vfunc_34(std::uint32_t)
    {
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
