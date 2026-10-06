#include "wxSpiderAttackState.h"
#include "Analysis/Host/wxSpiderAttackStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxSpiderAttackState>(); }
        const spRTTIRecord Record{wxSpiderAttackState::ClassID, wxCharacterState::ClassID,
            "wxSpiderAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxSpiderAttackStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxSpiderAttackStateHost*>(&host);
            if (!typed) throw std::logic_error("wxSpiderAttackState requires a consumer/event/owner host");
            return *typed;
        }
    }
    wxSpiderAttackState::wxSpiderAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxSpiderAttackState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxSpiderAttackState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxSpiderAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxSpiderAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxSpiderAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC520530 / PS22F18E0: consumer rate precedes control/key reads.
        auto& host = Host(RequireHostForAnalysis());
        host.SetConsumerRateForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
        const auto airborne = host.OwnerControlByte20ForAnalysis(GetOwnerForAnalysis());
        if (airborne)
            request.packedKey = (request.packedKey & 0xffff847fu) | 0x400u;
        else
            request.packedKey = (request.packedKey & 0xffff84ffu) | 0x480u;
        request.packedKey &= 0xf0007f80u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        // The entry neither releases old pending nor skips null/same handles.
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxSpiderAttackState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxSpiderAttackState::vfunc_34(std::uint32_t)
    {
        // PC5203D0 / PS22F1C30: null pending returns before host access.
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxSpiderAttackState::vfunc_3C(const void* event)
    {
        // PC520410 / PS22F1A70: exact complete strings, same action for both.
        auto& host = Host(RequireHostForAnalysis());
        const auto name = host.EventTagNameForAnalysis(event);
        if (name == "event_impact_begin" || name == "event_impact_end")
        {
            if (void* const receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
                host.SendImpactNotificationForAnalysis(receiver, *this, name == "event_impact_begin");
        }
        else if (name == "event_web" || name == "event_spit")
            host.TriggerOwnerActionForAnalysis(GetOwnerForAnalysis(), 0);
    }
}
