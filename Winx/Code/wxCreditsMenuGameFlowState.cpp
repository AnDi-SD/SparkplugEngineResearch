#include "wxCreditsMenuGameFlowState.h"
#include <limits>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxCreditsMenuGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("wxCreditsMenuGameFlowState requires a factory host");
            return std::make_unique<wxCreditsMenuGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxCreditsMenuGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxCreditsMenuGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        void* Require(void* value)
        {
            if(!value)throw std::logic_error("wxCreditsMenuGameFlowState requires a borrowed service/input");
            return value; // Own guard; not a native lazy-singleton/fault model.
        }
    }
    wxCreditsMenuGameFlowState::wxCreditsMenuGameFlowState(wxCreditsMenuGameFlowStateHost& host)
        :wxGameFlowState(host),creditsHost_(host)
    {
        // This is a second member18 read, after parent construction. It must
        // not be replaced by the parent's earlier cached input30.
        credits_.cachedInput3C=creditsHost_.BorrowInputForAnalysis();
        creditsHost_.RegisterCreditsReceiverForAnalysis(*this,0x13);
    }
    wxCreditsMenuGameFlowState::~wxCreditsMenuGameFlowState()
    {
        creditsHost_.UnregisterCreditsReceiverForAnalysis(*this,0x13);
    }
    void wxCreditsMenuGameFlowState::SetFactoryHostForAnalysis(wxCreditsMenuGameFlowStateHost* host) noexcept
    { factoryHost=host; }
    const spRTTIRecord& wxCreditsMenuGameFlowState::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxCreditsMenuGameFlowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxCreditsMenuGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxCreditsMenuGameFlowState>(creditsHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void wxCreditsMenuGameFlowState::FlowSlot20ForAnalysis()
    {
        auto* audio=Require(creditsHost_.ResolveCreditsAudioInterfaceForAnalysis());
        auto* controller=Require(creditsHost_.GetCreditsAudioControllerForAnalysis(audio));
        creditsHost_.SetCreditsAudioEnabledForAnalysis(controller,1);
        creditsHost_.BorrowCreditsSharedFlag45ForAnalysis()=0;
        auto* hud=Require(creditsHost_.ResolveCreditsHUDForAnalysis());
        creditsHost_.InvokeCreditsHUDForAnalysis(hud,0);
        auto* owner=Require(creditsHost_.ResolveHookInputOwnerForAnalysis());
        creditsHost_.ResetHookInputForAnalysis(Require(creditsHost_.ReadHookInputForAnalysis(owner)));
        creditsHost_.SetHookInputModeForAnalysis(Require(creditsHost_.ReadHookInputForAnalysis(owner)),0);
        credits_.field41=1;
    }
    void wxCreditsMenuGameFlowState::FlowSlot2CForAnalysis()
    {
        auto* owner=Require(creditsHost_.ResolveHookInputOwnerForAnalysis());
        creditsHost_.ResetHookInputForAnalysis(Require(creditsHost_.ReadHookInputForAnalysis(owner)));
        creditsHost_.SetHookInputModeForAnalysis(Require(creditsHost_.ReadHookInputForAnalysis(owner)),0);
    }
    void wxCreditsMenuGameFlowState::CompleteCreditsForAnalysis()
    {
        auto* controller=Require(creditsHost_.ResolveCreditsControllerForAnalysis());
        if(creditsHost_.ReadCreditsControllerState1B0ForAnalysis(controller)==0x4A)
        {
            creditsHost_.RequestCreditsControllerStateForAnalysis(controller,0x3C,0);
            // Native reloads the global only after this optional foreign call.
            controller=Require(creditsHost_.ResolveCreditsControllerForAnalysis());
        }
        creditsHost_.SetCreditsControllerStateForAnalysis(controller,0x38,0);
        auto* service=Require(creditsHost_.ResolveCreditsServiceForAnalysis());
        creditsHost_.SendCreditsServiceEventForAnalysis(service,0x2BC,0);
        auto* owner=Require(creditsHost_.ResolveHookInputOwnerForAnalysis());
        creditsHost_.BorrowCreditsInputOwnerFlag20ForAnalysis(owner)=1;
    }
    bool wxCreditsMenuGameFlowState::FlowSlot30ForAnalysis()
    {
        if(credits_.field41)
        {
            credits_.field41=0; // Store precedes the external request.
            auto* service=Require(creditsHost_.ResolveCreditsOnceServiceForAnalysis());
            creditsHost_.SendCreditsOnceRequestForAnalysis(service,0x50);
        }
        if(credits_.field40 && creditsHost_.ReadCreditsCachedInput2AForAnalysis(Require(credits_.cachedInput3C)))
        {
            credits_.field40=0;
            CompleteCreditsForAnalysis();
            return true;
        }
        // The original deliberately reloads cachedInput3C and flag2A on this
        // path, including after an earlier false test. Do not memoize it.
        if(!creditsHost_.ReadCreditsCachedInput2AForAnalysis(Require(credits_.cachedInput3C)))
            credits_.field40=1;
        return true;
    }
    void wxCreditsMenuGameFlowState::NotifyCreditsForAnalysis(const spNotificationForAnalysis& notification)
    {
        if(notification.code!=0x27DB)
        {
            wxGameFlowState::vfunc_0C(&notification);
            return;
        }
        if(notification.payload>std::numeric_limits<std::uint32_t>::max())
            throw std::domain_error("Credits numeric notification payload must fit its native32-bit word");
        if(notification.payload==0x50)CompleteCreditsForAnalysis();
    }
    void wxCreditsMenuGameFlowState::vfunc_0C(const void* portableNotification) noexcept
    {
        // Common portable notification frame, not the native byte layout.
        // This native virtual boundary has no C++ exception propagation seam.
        try
        {
            if(!portableNotification)throw std::invalid_argument("Credits notification is null");
            NotifyCreditsForAnalysis(*static_cast<const spNotificationForAnalysis*>(portableNotification));
        }
        catch(...) { std::terminate(); }
    }
}
