#include "wxTrixAttackState.h"
#include "Analysis/Host/wxTrixAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxTrixAttackState>(); }
        const spRTTIRecord record{wxTrixAttackState::ClassID, wxCharacterState::ClassID,
            "wxTrixAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxTrixAttackState::wxTrixAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxTrixAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxTrixAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxTrixAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxTrixAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxTrixAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC520330 / PS2313050: no release and no control-word clear.
        request.packedKey = (request.packedKey & 0xF007FF81u) | 1u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
    }
    bool wxTrixAttackState::vfunc_34(const std::uint32_t code)
    {
        // PC520100 / PS2313620: these three codes bypass the consumer.
        if (code == 0x1C || code == 0x11 || code == 0xA || !GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxTrixAttackState::vfunc_3C(const void* event)
    {
        // PC520150 / PS2313170: original ordered, full string comparisons.
        auto* host = dynamic_cast<wxTrixAttackStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxTrixAttackState requires an event host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxTrixAttackState host returned a null event tag name");
        struct EventMapping { const char* name; std::uint32_t code; };
        constexpr EventMapping events[] = {
            {"event_lightning", 0x2778}, {"event_spiral", 0x2777},
            {"event_iceshard", 0x2779}, {"event_thunder", 0x277A},
            {"event_icemine", 0x277C}, {"event_circles_begin", 0x277D},
            {"event_circles_end", 0x277D}, {"event_shieldbubble", 0x277E},
            {"icy_freeze", 0x2781},
        };
        for (const auto& mapping : events)
        {
            if (std::strcmp(name, mapping.name) != 0) continue;
            void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
            if (receiver)
            {
                if (mapping.code == 0x277D)
                    host->SendCirclesNotificationForAnalysis(receiver, *this,
                        std::strcmp(name, "event_circles_begin") == 0);
                else host->SendZeroNotificationForAnalysis(receiver, *this, mapping.code);
            }
            return;
        }
    }
}
