#include "wxPullLeverState.h"
#include "Analysis/Host/wxPullLeverStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<wxPullLeverState>(); }
        const spRTTIRecord Record{wxPullLeverState::ClassID, wxCharacterState::ClassID,
            "wxPullLeverState", &wxCharacterState::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxPullLeverStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed=dynamic_cast<wxPullLeverStateHost*>(&host);
            if(!typed) throw std::logic_error("wxPullLeverState requires a controller/player/notification host");
            return *typed;
        }
        void SendFlag(wxPullLeverStateHost& host,wxCharacterState& source,bool active)
        {
            void* const receiver=host.GlobalField2B4ReceiverForAnalysis();
            if(receiver) host.SendLeverFlagNotificationForAnalysis(receiver,source,active);
        }
    }
    wxPullLeverState::wxPullLeverState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxPullLeverState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxPullLeverState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxPullLeverState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxPullLeverState>(); manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager)) return nullptr;
        return clone;
    }
    bool wxPullLeverState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host=Host(RequireHostForAnalysis());
        request.packedKey=(request.packedKey&0xF0000980u)|0x980u;
        void* const handle=host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,false,true);
        // PC captures owner before the pending store, then clears its control.
        void* const controlOwner=GetOwnerForAnalysis(); SetPendingHandleFromState(handle);
        host.ClearOwnerActionControlForAnalysis(controlOwner);
        void* const entity=host.OwnerField124ForAnalysis(GetOwnerForAnalysis());
        host.SendFilteredNotificationForAnalysis(*this,0x272E,6,entity,0);
        host.SetOwnerEntityControllerFlagForAnalysis(GetOwnerForAnalysis(),true);
        void* const player=host.GlobalPlayerForAnalysis();
        host.ResetPlayerControllerForAnalysis(player);
        SendFlag(host,*this,true);
        return true;
    }
    bool wxPullLeverState::vfunc_20(wxAnimationRequestForAnalysis&)
    {
        auto& host=Host(RequireHostForAnalysis());
        host.SetOwnerEntityControllerFlagForAnalysis(GetOwnerForAnalysis(),false);
        SendFlag(host,*this,false); // No pending release or action clear on exit.
        return true;
    }
    void wxPullLeverState::vfunc_2C(wxAnimationRequestForAnalysis&)
    {
        ReleasePendingFromState(false);
        auto& host=Host(RequireHostForAnalysis());
        void* const owner=GetOwnerForAnalysis();
        const auto word=host.OwnerField218ForAnalysis(owner);
        host.CallOwnerField218BranchForAnalysis(owner,word!=0);
    }
    bool wxPullLeverState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
}
