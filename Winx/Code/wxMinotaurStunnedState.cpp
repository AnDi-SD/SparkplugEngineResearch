#include "wxMinotaurStunnedState.h"
#include "Analysis/Host/wxCharacterControlStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMinotaurStunnedState()
        { return std::make_unique<wxMinotaurStunnedState>(); }
        const spRTTIRecord record{wxMinotaurStunnedState::ClassID, wxCharacterState::ClassID,
            "wxMinotaurStunnedState", &wxCharacterState::StaticRTTI(),
            &CreateMinotaurStunnedState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMinotaurStunnedState::wxMinotaurStunnedState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMinotaurStunnedState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxMinotaurStunnedState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMinotaurStunnedState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMinotaurStunnedState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMinotaurStunnedState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 521BA0 / PS2 305A40: this entry overwrites +24 without release.
        request.packedKey = (request.packedKey & 0xF0010000u) | 0x00010000u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxMinotaurStunnedState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        auto* host = dynamic_cast<wxCharacterControlStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxMinotaurStunnedState requires a control-state host");
        host->ClearOwnerControlByteForAnalysis(GetOwnerForAnalysis(), 0x60);
        ClearOwnerActionControlFromState();
    }
    bool wxMinotaurStunnedState::vfunc_34(const std::uint32_t target)
    {
        auto& host = RequireHostForAnalysis();
        if (target == 11 && !host.IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            ReleasePendingFromState();
            return true;
        }
        // The first query consumes records. The native second query must run
        // independently even if the first one just returned true.
        return host.IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
