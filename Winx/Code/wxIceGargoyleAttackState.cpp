#include "wxIceGargoyleAttackState.h"
#include "Analysis/Host/wxIceGargoyleAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceGargoyleAttackState>(); }
        const spRTTIRecord record{wxIceGargoyleAttackState::ClassID, wxCharacterState::ClassID,
            "wxIceGargoyleAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceGargoyleAttackState::wxIceGargoyleAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceGargoyleAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceGargoyleAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceGargoyleAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceGargoyleAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxIceGargoyleAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC519DA0 / PS22EE890. Inherited Reset leaves field3C until update.
        if (GetTransitionFlag1D())
        {
            field3C_ = false;
            ClearTransitionFlag1D();
        }
        request.packedKey = (request.packedKey & 0xF007FFD1u) | 0x51u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
    }
    bool wxIceGargoyleAttackState::vfunc_34(const std::uint32_t code)
    {
        // PC520E20 / PS22EEB50. Null pending bypasses completion query.
        if (code == 0xA || !GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxIceGargoyleAttackState::vfunc_3C(const void* event)
    {
        // PC519D00 / PS22EE9C0. Complete case-sensitive string matches.
        auto* host = dynamic_cast<wxIceGargoyleAttackStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxIceGargoyleAttackState requires an event/controller host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxIceGargoyleAttackState host returned a null event tag name");
        if (std::strcmp(name, "event_shoot") == 0)
        {
            void* const controller = host->OwnerEntityField140ForAnalysis(GetOwnerForAnalysis());
            if (controller) host->InvokeControllerSlot38ForAnalysis(controller, 0);
            field3C_ = true;
            return;
        }
        const bool begin = std::strcmp(name, "event_damage_begin") == 0;
        if (!begin && std::strcmp(name, "event_damage_end") != 0) return;
        void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host->SendNamedFlagNotificationForAnalysis(receiver, *this, "hand_right", begin);
    }
}
