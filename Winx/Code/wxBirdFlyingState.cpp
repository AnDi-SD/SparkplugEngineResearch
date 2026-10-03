#include "wxBirdFlyingState.h"
#include "Analysis/Host/wxBirdFlyingStateHost.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<wxBirdFlyingState>();}
        const spRTTIRecord Record{wxBirdFlyingState::ClassID,wxCharacterState::ClassID,
            "wxBirdFlyingState",&wxCharacterState::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    wxBirdFlyingState::wxBirdFlyingState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxBirdFlyingState::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& wxBirdFlyingState::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> wxBirdFlyingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxBirdFlyingState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;return clone;
    }
    void wxBirdFlyingState::Play(wxAnimationRequestForAnalysis& request,std::uint32_t mask,std::uint32_t bits,bool mode)
    {
        ReleasePendingFromState();request.packedKey=(request.packedKey&mask)|bits;
        void* const handle=RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,mode,true);SetPendingHandleFromState(handle);
    }
    bool wxBirdFlyingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        if(GetTransitionFlag1C())
        {Play(request,0xf03fffdf,0x00200050,false);ClearTransitionFlag1C();}
        else if(RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true))
            return wxCharacterState::vfunc_1C(request);
        ClearOwnerActionControlFromState();return false;
    }
    bool wxBirdFlyingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if(GetTransitionFlag1E())
        {Play(request,0xf05fffd1,0x00400051,false);ClearTransitionFlag1E();}
        else if(RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true))
            return wxCharacterState::vfunc_20(request);
        ClearOwnerActionControlFromState();return false;
    }
    void wxBirdFlyingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if(GetTransitionFlag1D())
        {Play(request,0xf01fffdf,0x50,true);ClearTransitionFlag1D();}
    }
    void wxBirdFlyingState::vfunc_3C(const void* event)
    {
        auto* host=dynamic_cast<wxBirdFlyingStateHost*>(&RequireHostForAnalysis());
        if(!host)throw std::logic_error("BirdFlyingState requires an event/owner adapter");
        const auto* name=host->EventTagNameForAnalysis(event);
        if(!name)throw std::logic_error("BirdFlyingState event name is null");
        if(std::strcmp(name,"event_takeoff")==0)host->ClearTakeoffFlagForAnalysis(GetOwnerForAnalysis());
    }
}
