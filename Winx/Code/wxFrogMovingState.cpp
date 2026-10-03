#include "wxFrogMovingState.h"
#include "Analysis/Host/wxFrogMovingStateHost.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxFrogMovingState>(); }
        const spRTTIRecord record{wxFrogMovingState::ClassID, wxCharacterState::ClassID,
            "wxFrogMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFrogMovingState::wxFrogMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFrogMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFrogMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFrogMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFrogMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxFrogMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC 520D50 / PS2 2F4730. Motion is the external control float+4.
        auto* host = dynamic_cast<wxCharacterMotionStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxFrogMovingState requires a motion host");
        if (host->OwnerMotionForAnalysis(GetOwnerForAnalysis()) < 0.1f)
        {
            request.packedKey &= 0xFFFFFF8Fu;
            ClearOwnerActionControlFromState();
        }
        else
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        request.packedKey &= 0xF007FFFFu;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        // Original update queues the new handle without releasing old pending.
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
        // PC equality and unordered select movement. PS2 qualification is
        // limited to normal finite float32 and signed zeros.
    }
    void wxFrogMovingState::vfunc_3C(const void* event)
    {
        // PC 520CE0 / PS2 2F4880; case-sensitive complete string equality.
        auto* host = dynamic_cast<wxFrogMovingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxFrogMovingState requires an event host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxFrogMovingState host returned a null event tag name");
        if (std::strcmp(name, "event_hop_end") != 0) return;
        void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host->SendTagNotificationForAnalysis(receiver, *this, 0x27A4, name);
    }
}
