#include "wxDroidHurtState.h"
#include "Analysis/Host/wxDroidHurtStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<wxDroidHurtState>(); }
        const spRTTIRecord Record{wxDroidHurtState::ClassID, wxCharacterState::ClassID,
            "wxDroidHurtState", &wxCharacterState::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    wxDroidHurtState::wxDroidHurtState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDroidHurtState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxDroidHurtState::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> wxDroidHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDroidHurtState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDroidHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxDroidHurtStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("DroidHurtState requires an owner/clock/message adapter");
        request.packedKey = (request.packedKey & 0xff808000u) | 0x8000u;
        const auto raw = host->ReadHurtControlByteForAnalysis(GetOwnerForAnalysis());
        const std::uint32_t variant = host->MovementNumericProfileForAnalysis()
            == wxCharacterMovementNumericProfileForAnalysis::PS2Finite ? (raw & 31u) : (raw != 0);
        request.packedKey = (request.packedKey & 0xf07fffffu) | (variant << 23);
        host->ClearHurtControlByteForAnalysis(GetOwnerForAnalysis());
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        // Original entry overwrites pending without releasing the old handle.
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        deadline3C_ = host->ClockWordForAnalysis() + 1500u;
        word40_ = 0;
        ClearTransitionFlag1C();
        if (host->HasHurtMessageReceiverForAnalysis(GetOwnerForAnalysis()))
        {
            const wxDroidHurtStateHost::MessageForAnalysis message{
                0x27d1, {0, 0, 0}, this, 0, 0x6c, 0};
            host->SendHurtMessageForAnalysis(GetOwnerForAnalysis(), message);
        }
        ClearOwnerActionControlFromState();
        return true;
    }
    bool wxDroidHurtState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // Shared PC514550/PS23122B0, then the character-state exit.
        ClearOwnerActionControlFromState();
        return wxCharacterState::vfunc_20(request);
    }
    void wxDroidHurtState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        ClearOwnerActionControlFromState();
        if (GetTransitionFlag1D())
        {
            ClearTransitionFlag1D();
            PrepareMovementFromState(false);
        }
        FinishMovementFromState(false);
    }
    bool wxDroidHurtState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
