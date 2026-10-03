#include "wxYetiAttackState.h"
#include "Analysis/Host/wxYetiAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxYetiAttackState>(); }
        const spRTTIRecord record{wxYetiAttackState::ClassID, wxCharacterState::ClassID,
            "wxYetiAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxYetiAttackState::wxYetiAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxYetiAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxYetiAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxYetiAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxYetiAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxYetiAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC519FB0 / PS22EFAE0: store occurs even when the handle is unchanged.
        request.packedKey &= 0xF01FFF8Fu;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState();
            QueuePendingFromState(handle, false, true);
        }
        SetPendingHandleFromState(handle);
    }
    bool wxYetiAttackState::vfunc_34(const std::uint32_t code)
    {
        // PC519E50 / PS22EFD40 retains the selector-dependent branch.
        if (GetStateSelectorForAnalysis() != 0xA && (code == 0xA || code == 0x21)) return true;
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxYetiAttackState::vfunc_3C(const void* event)
    {
        // PC519EA0 / PS22EFBD0: one substring comparison among exact matches.
        auto* host = dynamic_cast<wxYetiAttackStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxYetiAttackState requires an event/controller host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxYetiAttackState host returned a null event tag name");
        if (std::strcmp(name, "yeti_backspike_attack") == 0)
        {
            void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
            if (receiver) host->SendBackspikeNotificationForAnalysis(receiver, *this);
            return;
        }
        if (std::strcmp(name, "event_shoot") == 0)
        {
            host->InvokeControllerSlot38ForAnalysis(host->OwnerEntityField140ForAnalysis(GetOwnerForAnalysis()), 0);
            return;
        }
        if (std::strstr(name, "event_blast_begin"))
        {
            host->InvokeControllerSlot3CForAnalysis(host->OwnerEntityField140ForAnalysis(GetOwnerForAnalysis()), 1, 1);
            return;
        }
        if (std::strcmp(name, "event_blast_end") == 0)
            host->InvokeControllerSlot3CForAnalysis(host->OwnerEntityField140ForAnalysis(GetOwnerForAnalysis()), 0, 1);
    }
}
