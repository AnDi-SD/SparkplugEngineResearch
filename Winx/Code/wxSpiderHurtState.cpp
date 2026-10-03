#include "wxSpiderHurtState.h"
#include "Analysis/Host/wxSpiderHurtStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<wxSpiderHurtState>(); }
        const spRTTIRecord Record{wxSpiderHurtState::ClassID, wxCharacterState::ClassID,
            "wxSpiderHurtState", &wxCharacterState::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxSpiderHurtStateHost& SpiderHost(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxSpiderHurtStateHost*>(&host);
            if (!result) throw std::logic_error("SpiderHurtState requires owner/consumer/clock adapter");
            return *result;
        }
    }
    wxSpiderHurtState::wxSpiderHurtState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxSpiderHurtState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxSpiderHurtState::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> wxSpiderHurtState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxSpiderHurtState>();manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;return clone;
    }
    bool wxSpiderHurtState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1C())
        {
            request.packedKey &= 0xf07fffffu;
            auto& host = SpiderHost(RequireHostForAnalysis());
            host.SetConsumerRateForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            runtime_.flag3C = false;
            request.packedKey = (request.packedKey & 0xffa08000u) | 0x208000u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            // Native first entry overwrites pending without releasing it.
            QueuePendingFromState(handle, false, true);SetPendingHandleFromState(handle);
            runtime_.permission4C = false;ClearTransitionFlag1C();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            if (runtime_.flag3D) runtime_.flag3C = true;
            return wxCharacterState::vfunc_1C(request);
        }
        ClearOwnerActionControlFromState();return false;
    }
    bool wxSpiderHurtState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1E())
        {
            request.packedKey = (request.packedKey & 0xffc08000u) | 0x408000u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            // Original lookup precedes release of the previous animation.
            ReleasePendingFromState();QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);ClearTransitionFlag1E();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_20(request);
        ClearOwnerActionControlFromState();return false;
    }
    void wxSpiderHurtState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        ClearOwnerActionControlFromState();
        auto& host = SpiderHost(RequireHostForAnalysis());
        if (GetTransitionFlag1D())
        {
            request.packedKey = (request.packedKey & 0xff808000u) | 0x8000u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, true, true);SetPendingHandleFromState(handle);
            runtime_.deadline48 = host.ClockWordForAnalysis() + runtime_.delay44;
            ClearTransitionFlag1D();
        }
        else if (runtime_.deadline48 < host.ClockWordForAnalysis())
        {
            runtime_.permission4C = true;runtime_.flag3C = true;
        }
    }
    bool wxSpiderHurtState::vfunc_34(std::uint32_t)
    {
        if (!SpiderHost(RequireHostForAnalysis()).OwnerPermissionGateForAnalysis(GetOwnerForAnalysis())) return true;
        return runtime_.permission4C;
    }
}
