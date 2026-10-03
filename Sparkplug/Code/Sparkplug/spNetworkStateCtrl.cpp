#include "spNetworkStateCtrl.h"
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkStateCtrlHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNetworkStateCtrl>();}
        const spRTTIRecord Record{spNetworkStateCtrl::ClassID,spBaseObject::ClassID,
            "spNetworkStateCtrl",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        class Lock final
        {
        public:
            Lock(spNetworkStateCtrlHost& host,spNetworkStateCtrlHost::Handle handle):host_(host),handle_(handle){host_.EnterLock(handle_);}
            ~Lock(){host_.LeaveLock(handle_);}
        private:spNetworkStateCtrlHost& host_;spNetworkStateCtrlHost::Handle handle_;
        };
    }
    void SetNetworkStateCtrlFactoryHostForAnalysis(std::shared_ptr<spNetworkStateCtrlHost> h){FactoryHost=std::move(h);}
    std::shared_ptr<spNetworkStateCtrlHost> GetNetworkStateCtrlFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkStateCtrl::spNetworkStateCtrl(std::shared_ptr<spNetworkStateCtrlHost> h):host_(std::move(h))
    {
        if(!host_)throw std::invalid_argument("NetworkStateCtrl requires a foreign-service host");
        lock_=host_->CreateLock();
        if(!lock_)throw std::runtime_error("NetworkStateCtrl lock is unavailable");
    }
    spNetworkStateCtrl::~spNetworkStateCtrl()
    {
        // Original lock storage release precedes all queue node releases;
        // no virtual lock destructor or Enter/Leave runs here.
        host_->FreeLockStorage(lock_);
        queue_.clear();
    }
    const spRTTIRecord& spNetworkStateCtrl::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetworkStateCtrl::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkStateCtrl::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spNetworkStateCtrl>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    void spNetworkStateCtrl::RequestForAnalysis(std::uint32_t event,std::uint8_t immediate)
    {
        const Lock lock(*host_,lock_);
        if(immediate)(void)ApplyImmediateForAnalysis(event);
        else queue_.push_back(event);
    }
    void spNetworkStateCtrl::SetTransition(std::uint32_t target,std::uint32_t result) noexcept
    {previous_=current_;target_=target;changed_=1;result_=result;}
    void spNetworkStateCtrl::EnterState(std::uint32_t state)
    {
        switch(state)
        {
        case 1:(void)host_->StartConnection(host_->ResolveNetworkManager());break;
        case 2:
            host_->ManagerOperation450B70(host_->ResolveNetworkManager());
            host_->ManagerOperation4516F0(host_->ResolveNetworkManager(),1);break;
        case 3:host_->RefreshTimer(host_->ResolveNetworkManager());break;
        default:break;
        }
    }
    bool spNetworkStateCtrl::Evaluate(std::uint32_t event)
    {
        // Original004F5D30 never clears changed/result on rejected inputs.
        switch(current_)
        {
        case 0:if(event==4){SetTransition(1,0);return true;}break;
        case 1:
            if(event==0){EnterState(1);return true;}
            if(event==6){SetTransition(4,3);return true;}
            if(event==8){SetTransition(4,4);return true;}
            if(event==5){SetTransition(2,0);return true;}break;
        case 2:
            if(event==0){EnterState(2);return true;}
            if(event==8){SetTransition(4,4);return true;}
            if(event==7){SetTransition(3,0);return true;}break;
        case 3:
            if(event==0){EnterState(3);return true;}
            if(event==8){SetTransition(4,4);return true;}break;
        default:break;
        }
        return false;
    }
    std::uint32_t spNetworkStateCtrl::ApplyImmediateForAnalysis(std::uint32_t event)
    {
        (void)Evaluate(event);
        if(result_)return result_;
        if(changed_){current_=target_;EnterState(current_);}
        return result_; // Entry callbacks can have changed this same object.
    }
    void spNetworkStateCtrl::PumpForAnalysis()
    {
        std::uint32_t event;
        {
            const Lock lock(*host_,lock_);
            if(queue_.empty())return;
            event=queue_.front();queue_.pop_front();
        }
        (void)Evaluate(event);
        // Unlike Immediate, Pump ignores a nonzero result and commits target.
        if(changed_){current_=target_;EnterState(current_);}
    }
    spNetworkStateCtrl::StateForAnalysis spNetworkStateCtrl::GetStateForAnalysis() const
    {return {changed_,target_,previous_,current_,result_,{queue_.begin(),queue_.end()}};}
    void spNetworkStateCtrl::SetStateForAnalysis(std::uint8_t changed,std::uint32_t target,
        std::uint32_t previous,std::uint32_t current,std::uint32_t result) noexcept
    {changed_=changed;target_=target;previous_=previous;current_=current;result_=result;}
}
