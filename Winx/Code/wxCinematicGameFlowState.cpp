#include "wxCinematicGameFlowState.h"
#include "Code/SparkBase/spBroadcastNotification.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <exception>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxCinematicGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("Cinematic requires a factory host");
            return std::make_unique<wxCinematicGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxCinematicGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxCinematicGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        spSubscriptionManager& Subscriptions()
        {
            auto* manager=spSubscriptionManager::GetInstance();
            if(!manager)throw std::logic_error("Cinematic requires an existing shared SubscriptionManager");
            return *manager;
        }
        void* Require(void* value)
        {
            if(!value)throw std::logic_error("Cinematic requires a borrowed input owner/device");
            return value; // Analytical guard, not native lazy-singleton/fault behavior.
        }
    }
    wxCinematicGameFlowState::wxCinematicGameFlowState(wxCinematicGameFlowStateHost& host)
        :wxGameFlowState(host),cinematicHost_(host)
    {
        (void)Subscriptions().SubscribeForAnalysis(0x10,*this);
        ResetCinematicSelectionsForAnalysis();
    }
    wxCinematicGameFlowState::~wxCinematicGameFlowState()
    {
        // Native resolves the current manager again. The qualified profile
        // requires application-owned manager lifetime across this operation.
        (void)Subscriptions().UnsubscribeForAnalysis(0x10,*this);
    }
    void wxCinematicGameFlowState::SetFactoryHostForAnalysis(wxCinematicGameFlowStateHost* host) noexcept
    { factoryHost=host; }
    const spRTTIRecord& wxCinematicGameFlowState::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxCinematicGameFlowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxCinematicGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxCinematicGameFlowState>(cinematicHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void wxCinematicGameFlowState::ResetCinematicSelectionsForAnalysis()
    {
        auto& flags=cinematicHost_.CinematicFlagsForAnalysis();
        for(std::size_t i=0;i<flags.size();++i)
        {
            flags[i]&=0xFFFFFFFE;
            cinematic_.flags8C[i]=0;
        }
        cinematic_.field84=0;
    }
    void wxCinematicGameFlowState::NotifyCinematicForAnalysis(const spNotificationForAnalysis& message) noexcept
    {
        if(message.code==0x27A3 && (message.payload&0xFF)==0)
            cinematic_.field84=0;
    }
    void wxCinematicGameFlowState::vfunc_0C(const void* message) noexcept
    {
        if(!message)std::terminate(); // Portable guard; native dereferences the frame.
        NotifyCinematicForAnalysis(*static_cast<const spNotificationForAnalysis*>(message));
    }
    void wxCinematicGameFlowState::FlowSlot20ForAnalysis()
    {
        auto* owner=Require(cinematicHost_.ResolveCinematicInputOwnerForAnalysis());
        cinematicHost_.ResetCinematicInputForAnalysis(Require(cinematicHost_.ReadCinematicInput1CForAnalysis(owner)));
        // The owner remains captured while its member1C is reloaded after
        // Reset. Replacing the global owner in that callback has no effect.
        cinematicHost_.SetCinematicInputModeForAnalysis(Require(cinematicHost_.ReadCinematicInput1CForAnalysis(owner)),6);
        cinematic_.field84=1;
        cinematic_.fieldE0=0;
    }
    void wxCinematicGameFlowState::FlowSlot24ForAnalysis()
    {
        const auto nativeReceiver=cinematicHost_.CinematicNativeReceiverWordForAnalysis(*this);
        if(!nativeReceiver || !*nativeReceiver)
            throw std::logic_error("Cinematic Exit requires a captured native32 receiver binding");
        // PUSH ECX initializes the scratch word; MOV byte[scratch],E0 changes
        // only its low byte. Native snapshots E0 BEFORE word7C. Never derive
        // the high24 bits from this portable C++ object's address.
        const std::uint32_t tail=(*nativeReceiver&0xFFFFFF00)|cinematic_.fieldE0;
        const std::uint32_t payload=cinematic_.field7C;
        if(SendBroadcastNotificationForAnalysis(this,0x27DC,0x13,payload,tail)
            !=spBroadcastNotificationResultForAnalysis::Completed)
            throw std::logic_error("Cinematic Exit requires an existing shared SubscriptionManager");
        auto* owner=Require(cinematicHost_.ResolveCinematicInputOwnerForAnalysis());
        cinematicHost_.ResetCinematicInputForAnalysis(Require(cinematicHost_.ReadCinematicInput1CForAnalysis(owner)));
        cinematic_.field84=0;
        cinematic_.fieldE0=0;
    }
    bool wxCinematicGameFlowState::FlowSlot30ForAnalysis()
    {
        throw std::logic_error("Cinematic movie/load slot30 remains unqualified");
    }
}
