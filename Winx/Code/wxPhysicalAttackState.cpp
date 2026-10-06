#include "wxPhysicalAttackState.h"
#include "Analysis/Host/wxPhysicalAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<wxPhysicalAttackState>(); }
        const spRTTIRecord Record{wxPhysicalAttackState::ClassID, wxCharacterState::ClassID,
            "wxPhysicalAttackState", &wxCharacterState::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxPhysicalAttackStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxPhysicalAttackStateHost*>(&host);
            if (!typed) throw std::logic_error("wxPhysicalAttackState requires an event/notification host");
            return *typed;
        }
    }
    wxPhysicalAttackState::wxPhysicalAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxPhysicalAttackState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxPhysicalAttackState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxPhysicalAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxPhysicalAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxPhysicalAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5176C0 / PS22D0B90: clear precedes the key rewrite and lookup.
        ClearOwnerActionControlFromState();
        request.packedKey &= 0xF007FF8Fu;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true); // Unconditional, no previous release.
        SetPendingHandleFromState(handle);
        auto& host = Host(RequireHostForAnalysis());
        void* const entity = host.OwnerField124ForAnalysis(GetOwnerForAnalysis());
        host.SendFilteredNotificationForAnalysis(*this, 0x2731, 6, entity, 0);
        return true;
    }
    void wxPhysicalAttackState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxPhysicalAttackState::vfunc_34(std::uint32_t)
    {
        // PC523770 / PS22D0E30: null is passed to the consuming query too.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    std::uint32_t wxPhysicalAttackState::vfunc_38(std::uint32_t code) const noexcept { return code == 9; }
    void wxPhysicalAttackState::vfunc_3C(const void* event)
    {
        auto& host = Host(RequireHostForAnalysis());
        const char* const name = host.EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxPhysicalAttackState event tag name is unavailable");
        bool begin;
        if (std::strcmp(name, "event_impact_begin") == 0) begin = true;
        else if (std::strcmp(name, "event_impact_end") == 0) begin = false;
        else return;
        void* const receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host.SendImpactNotificationForAnalysis(receiver, *this, "foot_left", begin);
    }
}
