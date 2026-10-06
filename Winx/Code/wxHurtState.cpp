#include "wxHurtState.h"
#include "Analysis/Host/wxHurtStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxHurtState>(); }
        const spRTTIRecord Record{wxHurtState::ClassID, wxCharacterState::ClassID,
            "wxHurtState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxHurtStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxHurtStateHost*>(&host);
            if (!typed) throw std::logic_error("wxHurtState requires an owner control/mode host");
            return *typed;
        }
    }
    wxHurtState::wxHurtState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxHurtState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxHurtState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxHurtState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        void* const control = host.OwnerHurtControlForAnalysis(GetOwnerForAnalysis());
        auto key = request.packedKey;
        switch (key & 0xfu)
        {
        case 1: case 3: case 4: case 6: case 7:
            key &= 0xf07fff8fu;
            break;
        case 5:
            key &= 0xf07fffffu;
            break;
        case 8:
            key &= 0xfffffff0u;
            break;
        default:
            if (byte3C_ && host.HurtControlByte60ForAnalysis(control) && !(key & 0x180000u))
                key = (key & 0xf17fffdfu) | 0x1000050u;
            else
            {
                const auto motion = host.HurtControlMotionForAnalysis(control);
                if (motion < 0.2f)
                    key &= 0xf07fff8fu;
                else
                {
                    // PC rereads control+4 for the second compare; PS2
                    // keeps its first scalar F1. Ordered less handles NaN
                    // through the same final branch as the original tests.
                    const auto second = host.PlatformForAnalysis() == wxHurtStatePlatformForAnalysis::PC
                        ? host.HurtControlMotionForAnalysis(control) : motion;
                    if (second < 0.5f)
                        key = (key & 0xf07fffdfu) | 0x50u;
                    else
                        key = (key & 0xf0ffffdfu) | 0x800050u;
                }
            }
        }
        request.packedKey = (key & 0xff98807fu) | 0x8000u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxHurtState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxHurtState::vfunc_34(std::uint32_t)
    {
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    std::uint32_t wxHurtState::vfunc_38(std::uint32_t code) const
    {
        // Native reads mode before code dispatch, even for unconditional cases.
        // Missing analytical binding is an explicit host error.
        const auto mode = Host(RequireHostForAnalysis()).OwnerModeForAnalysis(GetOwnerForAnalysis());
        if (mode == 4 && code == 4) return false;
        switch (code)
        {
        case 0: case 19: case 20: case 21: case 22: case 23: case 24: case 29: case 30:
            return false;
        default:
            return true;
        }
    }
}
