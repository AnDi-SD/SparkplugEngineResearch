#include "wxLogoAnimGameFlowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxLogoAnimGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("wxLogoAnimGameFlowState requires a factory host");
            return std::make_unique<wxLogoAnimGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxLogoAnimGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxLogoAnimGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        template<class T>T* Require(T* value)
        {
            if(!value)throw std::logic_error("Logo hook requires a declared borrowed service/input");
            return value; // Analytical nonnull guard, not native lazy startup or fault behavior.
        }
    }
    wxLogoAnimGameFlowState::wxLogoAnimGameFlowState(wxLogoAnimGameFlowStateHost& host)
        :wxGameFlowState(host),logoHost_(host)
    {
        logo_.cachedInput3C=logoHost_.BorrowInputForAnalysis();
        logoHost_.RegisterLogoReceiverForAnalysis(*this,0x13);
    }
    wxLogoAnimGameFlowState::~wxLogoAnimGameFlowState()
    {
        logoHost_.UnregisterLogoReceiverForAnalysis(*this,0x13);
        timer_.reset();
        ReleaseLogoReferenceForAnalysis(false); // read50 follows unsubscribe and timer destruction
    }
    void wxLogoAnimGameFlowState::SetFactoryHostForAnalysis(wxLogoAnimGameFlowStateHost* host) noexcept{factoryHost=host;}
    const spRTTIRecord& wxLogoAnimGameFlowState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxLogoAnimGameFlowState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxLogoAnimGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxLogoAnimGameFlowState>(logoHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spTimer& wxLogoAnimGameFlowState::LogoTimerForAnalysis()
    {if(!timer_)throw std::logic_error("Logo inline timer lifetime has ended");return *timer_;}
    const spTimer& wxLogoAnimGameFlowState::LogoTimerForAnalysis() const
    {if(!timer_)throw std::logic_error("Logo inline timer lifetime has ended");return *timer_;}
    void wxLogoAnimGameFlowState::ReleaseLogoReferenceForAnalysis(bool clearAfter)
    {
        if(auto* value=logo_.referenced50)
        {
            auto& refs=logoHost_.BorrowLogoNativeReferenceCount8ForAnalysis(*value);
            refs=static_cast<std::uint16_t>(refs-1u);
            if(refs==0)logoHost_.DestroyLogoReferencedObjectForAnalysis(*value,1);
            if(clearAfter)logo_.referenced50=nullptr;
        }
    }
    void wxLogoAnimGameFlowState::FlowSlot20ForAnalysis()
    {
        auto* service=Require(logoHost_.ResolveLogoServiceForAnalysis());
        if(logoHost_.LogoHookProfileForAnalysis()==wxLogoAnimGameFlowStateHost::HookProfileForAnalysis::PS2)
            logoHost_.InvokePS2LogoServiceEntryForAnalysis(service);
        // PC service entry48EAA0 is a literal RET; there is no substituted call.
        auto* audio=Require(logoHost_.ResolveLogoAudioInterfaceForAnalysis());
        auto* controller=Require(logoHost_.GetLogoAudioControllerForAnalysis(audio));
        logoHost_.SetLogoAudioEnabledForAnalysis(controller,0);
        auto* owner=Require(logoHost_.ResolveHookInputOwnerForAnalysis());
        logoHost_.ResetHookInputForAnalysis(Require(logoHost_.ReadHookInputForAnalysis(owner)));
        logoHost_.SetHookInputModeForAnalysis(Require(logoHost_.ReadHookInputForAnalysis(owner)),0);
        MutableStateForAnalysis().field25=0;
        // Original597480 body is implemented as a standalone operation whose
        // lost C++ owner remains unknown; never an unconditional host success.
        LoadLogoSharedResourcesForAnalysis(logoHost_);
        const spTimer::ClockSourceForAnalysis clock=[this]{return logoHost_.ReadLogoTimerClockForAnalysis();};
        (void)LogoTimerForAnalysis().ResetForAnalysis(clock);
        (void)LogoTimerForAnalysis().StartForAnalysis(clock); // second, independent clock sample
        owner=Require(logoHost_.ResolveLogoAppOwnerForAnalysis());
        bool clear=logoHost_.ReadLogoAppByte4BForAnalysis(owner)!=0;
        if(!clear)
        {
            // PC retains the captured owner. PS2 reloads the global before60.
            if(logoHost_.LogoHookProfileForAnalysis()==wxLogoAnimGameFlowStateHost::HookProfileForAnalysis::PS2)
                owner=Require(logoHost_.ResolveLogoAppOwnerForAnalysis());
            clear=logoHost_.ReadLogoAppWord60ForAnalysis(owner)!=0;
        }
        if(clear)logoHost_.BorrowLogoGlobalWordsForAnalysis().fill(0);
    }
    void wxLogoAnimGameFlowState::FlowSlot24ForAnalysis()
    {
        auto* service=Require(logoHost_.ResolveLogoServiceForAnalysis());
        if(logoHost_.LogoHookProfileForAnalysis()==wxLogoAnimGameFlowStateHost::HookProfileForAnalysis::PS2)
            logoHost_.InvokePS2LogoServiceExitForAnalysis(service,"GLOBAL");
        // PC service exit5B7A00 is a literal RET4 with the GLOBAL argument.
        ReleaseLogoReferenceForAnalysis(true);
    }
    void wxLogoAnimGameFlowState::FlowSlot2CForAnalysis()
    {
        auto* owner=Require(logoHost_.ResolveHookInputOwnerForAnalysis());
        logoHost_.ResetHookInputForAnalysis(Require(logoHost_.ReadHookInputForAnalysis(owner)));
        logoHost_.SetHookInputModeForAnalysis(Require(logoHost_.ReadHookInputForAnalysis(owner)),0);
    }
    bool wxLogoAnimGameFlowState::FlowSlot30ForAnalysis()
    {throw std::logic_error("Logo Slot30 resource/FPU transition body is outside the recovered contract");}
}
