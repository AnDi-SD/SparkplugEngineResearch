#include "wxGhoulJumpingState.h"
#include "Analysis/Host/wxGhoulJumpingStateHost.h"
#include "wxJumpingStateOperationsForAnalysis.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<wxGhoulJumpingState>(); }
        const spRTTIRecord Record{wxGhoulJumpingState::ClassID, wxCharacterState::ClassID,
            "wxGhoulJumpingState", &wxCharacterState::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxGhoulJumpingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed=dynamic_cast<wxGhoulJumpingStateHost*>(&host);
            if(!typed) throw std::logic_error("wxGhoulJumpingState requires an action/jump/owner-branch host");
            return *typed;
        }
    }
    wxGhoulJumpingState::wxGhoulJumpingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxGhoulJumpingState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxGhoulJumpingState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxGhoulJumpingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxGhoulJumpingState>(); manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager)) return nullptr;
        return clone;
    }
    bool wxGhoulJumpingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host=Host(RequireHostForAnalysis());
        jumping_state_operations::PrepareAnimationBaseKey(request);
        auto& control=host.OwnerDirectActionControlForAnalysis(GetOwnerForAnalysis());
        jumping_state_operations::PrepareAnimationVariant(request,control);
        void* const handle=host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,false,true);
        SetPendingHandleFromState(handle);
        return true;
    }
    void wxGhoulJumpingState::vfunc_2C(wxAnimationRequestForAnalysis&)
    {
        // PC shares the exact body51EC80 with PullLever.
        ReleasePendingFromState(false);
        auto& host=Host(RequireHostForAnalysis());
        void* const owner=GetOwnerForAnalysis();
        jumping_state_operations::DispatchOwner218(host,owner);
    }
    bool wxGhoulJumpingState::vfunc_34(std::uint32_t code)
    {
        if(code==0x17) return false;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
    std::uint32_t wxGhoulJumpingState::vfunc_38(std::uint32_t code) const noexcept
    {
        return jumping_state_operations::Permission38(code);
    }
    void wxGhoulJumpingState::vfunc_3C(const void* event)
    {
        auto& host=Host(RequireHostForAnalysis());
        const char* const name=host.EventTagNameForAnalysis(event);
        if(!name) throw std::logic_error("wxGhoulJumpingState event tag is null");
        // PC compares four bytes "air\0"; PS2 uses strcmp against "air".
        if(std::strcmp(name,"air")==0)
        {
            auto& control=host.OwnerEntityJumpControlForAnalysis(GetOwnerForAnalysis());
            control.velocity[0]=0; control.velocity[1]=375; control.velocity[2]=0;
            control.enabled=true;
        }
    }
}
