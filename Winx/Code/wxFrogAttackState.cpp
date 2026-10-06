#include "wxFrogAttackState.h"
#include "Analysis/Host/wxFrogAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState(){return std::make_unique<wxFrogAttackState>();}
        const spRTTIRecord Record{wxFrogAttackState::ClassID,wxCharacterState::ClassID,
            "wxFrogAttackState",&wxCharacterState::StaticRTTI(),&CreateState,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxFrogAttackStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed=dynamic_cast<wxFrogAttackStateHost*>(&host);
            if(!typed)throw std::logic_error("wxFrogAttackState requires an event/controller host");
            return *typed;
        }
    }
    wxFrogAttackState::wxFrogAttackState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxFrogAttackState::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& wxFrogAttackState::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> wxFrogAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxFrogAttackState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    void wxFrogAttackState::vfunc_0C(const void* notification) noexcept
    {
        // PC520A40 / PS22F3E40. Unknown codes do not resolve any host.
        if(*static_cast<const std::uint32_t*>(notification)!=0x275c||!GetPendingHandleForAnalysis())return;
        auto& host=Host(RequireHostForAnalysis());
        byte3C_=1;
        host.RestartReverseForAnalysis(GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis());
        // The receiver is read AFTER the controller callback.
        if(void* receiver=host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
            host.SendUnnamedFlagNotificationForAnalysis(receiver,*this,false);
    }
    bool wxFrogAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC520B00 / PS22F3BE0: notification precedes request mutation.
        auto& host=Host(RequireHostForAnalysis());
        if(void* receiver=host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
            host.SendUnnamedFlagNotificationForAnalysis(receiver,*this,true);
        request.packedKey=(request.packedKey&0xf0000400u)|0x400u;
        void* const handle=host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,false,true); // Always, including null/same.
        SetPendingHandleFromState(handle);
        vfunc_30(request); // Original virtual dispatch, after pending store.
        return true;
    }
    void wxFrogAttackState::vfunc_30(wxAnimationRequestForAnalysis&)
    {ClearOwnerActionControlFromState();}
    bool wxFrogAttackState::vfunc_34(std::uint32_t)
    {
        // No pending-null guard in this own slot. Consumer is always queried.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
    std::uint32_t wxFrogAttackState::vfunc_38(std::uint32_t) const noexcept{return false;}
    void wxFrogAttackState::vfunc_3C(const void* event)
    {
        auto& host=Host(RequireHostForAnalysis());
        const char* name=host.EventTagNameForAnalysis(event);
        if(!name)throw std::logic_error("wxFrogAttackState event tag name is unavailable");
        if(std::strcmp(name,"event_damage_end")!=0)return;
        if(void* receiver=host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
            host.SendUnnamedFlagNotificationForAnalysis(receiver,*this,false);
    }
}
