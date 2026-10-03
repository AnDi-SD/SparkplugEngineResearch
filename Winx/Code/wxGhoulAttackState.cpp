#include "wxGhoulAttackState.h"
#include "Analysis/Host/wxGhoulAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxGhoulAttackState>(); }
        const spRTTIRecord record{wxGhoulAttackState::ClassID, wxCharacterState::ClassID,
            "wxGhoulAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxGhoulAttackState::wxGhoulAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxGhoulAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxGhoulAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxGhoulAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxGhoulAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxGhoulAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC517B90 / PS22E4430: always queue, without release or handle compare.
        request.packedKey &= 0xF007FF8Fu;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        return true;
    }
    bool wxGhoulAttackState::vfunc_34(const std::uint32_t code)
    {
        // PC5179C0 / PS22E4870: two codes bypass completion records.
        if (code == 0xA || code == 9 || !GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxGhoulAttackState::vfunc_3C(const void* event)
    {
        // PC517A10 / PS22E4540: ordered full, case-sensitive matches.
        auto* host = dynamic_cast<wxGhoulAttackStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxGhoulAttackState requires an event/controller host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxGhoulAttackState host returned a null event tag name");
        struct Mapping { const char* event; const char* payload; bool flag; };
        constexpr Mapping flags[] = {
            {"event_impact_begin", "hand_left", true}, {"event_impact_end", "hand_left", false},
            {"event_kick_begin", "L_Toe", true}, {"event_kick_end", "L_Toe", false},
        };
        for (const auto& mapping : flags)
        {
            if (std::strcmp(name, mapping.event) != 0) continue;
            void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
            if (receiver) host->SendNamedFlagNotificationForAnalysis(receiver, *this, mapping.payload, mapping.flag);
            return;
        }
        if (std::strcmp(name, "event_air_begin") == 0)
        {
            host->InvokeControllerFloatServiceForAnalysis(host->OwnerEntityField12CForAnalysis(GetOwnerForAnalysis()), 200.0f);
            return;
        }
        if (std::strcmp(name, "event_throw") != 0) return;
        host->InvokeControllerSlot38ForAnalysis(host->OwnerEntityField140ForAnalysis(GetOwnerForAnalysis()), 0);
        // Receiver is read after the controller callback, including null.
        void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host->SendThrowNotificationForAnalysis(receiver, *this);
    }
}
