#include "wxOpenSecretPassageState.h"
#include "Analysis/Host/wxOpenSecretPassageStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<wxOpenSecretPassageState>();}
        const spRTTIRecord Record{wxOpenSecretPassageState::ClassID,wxCharacterState::ClassID,
            "wxOpenSecretPassageState",&wxCharacterState::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxOpenSecretPassageStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed=dynamic_cast<wxOpenSecretPassageStateHost*>(&host);
            if(!typed)throw std::logic_error("wxOpenSecretPassageState requires an event/notification host");
            return *typed;
        }
    }
    wxOpenSecretPassageState::wxOpenSecretPassageState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxOpenSecretPassageState::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& wxOpenSecretPassageState::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> wxOpenSecretPassageState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxOpenSecretPassageState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool wxOpenSecretPassageState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey=(request.packedKey&0xf0000d00u)|0xd00u;
        void* const handle=RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,false,true); // Always, without old-pending release.
        SetPendingHandleFromState(handle);
        // Own entry writes directly, without dispatching virtual update.
        ClearOwnerActionControlFromState();
        return true;
    }
    void wxOpenSecretPassageState::vfunc_30(wxAnimationRequestForAnalysis&)
    {ClearOwnerActionControlFromState();}
    bool wxOpenSecretPassageState::vfunc_34(std::uint32_t)
    {
        if(!GetPendingHandleForAnalysis())return true; // Native null shortcut.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
    std::uint32_t wxOpenSecretPassageState::vfunc_38(std::uint32_t) const noexcept{return false;}
    void wxOpenSecretPassageState::vfunc_3C(const void* event)
    {
        auto& host=Host(RequireHostForAnalysis());
        const char* name=host.EventTagNameForAnalysis(event);
        if(!name)throw std::logic_error("wxOpenSecretPassageState event tag name is unavailable");
        if(std::strcmp(name,"SND_INTERACTION")!=0)return;
        void* const entity=host.OwnerField124ForAnalysis(GetOwnerForAnalysis());
        host.SendFilteredNotificationForAnalysis(*this,0x2762,6,entity,0);
    }
}
