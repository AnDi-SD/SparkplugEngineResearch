#include "wxMosquitoAttackState.h"
#include "Analysis/Host/wxMosquitoAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<wxMosquitoAttackState>();}
        const spRTTIRecord Record{wxMosquitoAttackState::ClassID,wxCharacterState::ClassID,
            "wxMosquitoAttackState",&wxCharacterState::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxMosquitoAttackStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed=dynamic_cast<wxMosquitoAttackStateHost*>(&host);
            if(!typed)throw std::logic_error("wxMosquitoAttackState requires an event/entity-controller host");
            return *typed;
        }
    }
    wxMosquitoAttackState::wxMosquitoAttackState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxMosquitoAttackState::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& wxMosquitoAttackState::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> wxMosquitoAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxMosquitoAttackState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool wxMosquitoAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey=(request.packedKey&0xf0000401u)|0x401u;
        void* const handle=RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,false,true); // Always, even null or already pending.
        SetPendingHandleFromState(handle);
        vfunc_30(request); // Original virtual dispatch after the pending store.
        return true;
    }
    void wxMosquitoAttackState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        ClearOwnerActionControlFromState();
        // Original reloads owner after clearing direct action control.
        Host(RequireHostForAnalysis()).ResetOwnerEntityControllerForAnalysis(GetOwnerForAnalysis());
    }
    bool wxMosquitoAttackState::vfunc_34(std::uint32_t)
    {
        // Native consumer query also runs when pending is null.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
    std::uint32_t wxMosquitoAttackState::vfunc_38(std::uint32_t) const noexcept{return false;}
    void wxMosquitoAttackState::vfunc_3C(const void* event)
    {
        auto& host=Host(RequireHostForAnalysis());
        const char* name=host.EventTagNameForAnalysis(event);
        if(!name)throw std::logic_error("wxMosquitoAttackState event tag name is unavailable");
        if(std::strcmp(name,"event_shoot")!=0)return;
        host.CallOwnerEntityField140Slot38ForAnalysis(GetOwnerForAnalysis(),false);
    }
}
