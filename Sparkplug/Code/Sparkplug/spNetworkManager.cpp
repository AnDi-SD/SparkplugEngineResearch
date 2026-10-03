#include "spNetworkManager.h"
#include "../../Analysis/PC/spNetworkManagerAbi.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkManagerHost> FactoryHost;
        spNetworkManager* Current=nullptr;
        std::vector<std::unique_ptr<spNetworkManager>> LazyStorage;
        std::array<std::uint32_t,5> LatencySamples{};
        std::uint8_t NextLatencySample=0;
        constexpr const char* Errors[]={"No error","Invalid Unique ID return by Host",
            "Unable to connect to the Host","Unable to start the server thread",
            "Problem with the Host connection","Problem with the Matchmaking connection",
            "Lost peer connection","Some connected peers are not ready to start",
            "Zero bytes sent - Lost connection","Cannot get valid configuration from Host"};
        class Lock final
        {
        public:
            Lock(spNetworkManagerHost& host,std::uintptr_t handle):host_(host),handle_(handle){host_.EnterLock(handle_);}
            ~Lock(){host_.LeaveLock(handle_);}
        private:spNetworkManagerHost& host_;std::uintptr_t handle_;
        };
        std::uint32_t Word(const std::vector<std::uint8_t>& data,std::size_t offset)
        {
            if(data.size()<offset+4)throw std::length_error("NetworkManager packet read exceeds supplied payload");
            return std::uint32_t(data[offset])|(std::uint32_t(data[offset+1])<<8)|
                (std::uint32_t(data[offset+2])<<16)|(std::uint32_t(data[offset+3])<<24);
        }
        std::int64_t Signed(std::uint32_t v){return v<0x80000000u?v:std::int64_t(v)-0x100000000LL;}
    }
    void SetNetworkManagerFactoryHostForAnalysis(std::shared_ptr<spNetworkManagerHost> h){FactoryHost=std::move(h);}
    std::shared_ptr<spNetworkManagerHost> GetNetworkManagerFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkManager* spNetworkManager::CurrentForAnalysis() noexcept{return Current;}
    spNetworkManager& spNetworkManager::GetOrCreateForAnalysis()
    {
        if(!Current){auto object=std::make_unique<spNetworkManager>();LazyStorage.push_back(std::move(object));}
        return *Current;
    }
    void spNetworkManager::ReleaseLazyStorageForAnalysis() noexcept{LazyStorage.clear();}
    spNetworkManager::spNetworkManager(std::shared_ptr<spNetworkManagerHost> host):host_(std::move(host))
    {
        if(!host_)throw std::logic_error("NetworkManager host is missing");
        queueLock_=host_->CreateQueueLock();
        if(!queueLock_)throw std::logic_error("NetworkManager queue lock is missing");
        for(std::size_t i=0;i<errors_.size();++i)errors_[i]=Errors[i];
        // The native singleton subobject writes the global during construction.
        // Publish only after portable construction succeeds (host failure guard).
        Current=this;
    }
    spNetworkManager::~spNetworkManager()
    {
        StopForAnalysis();
        host_->FreeLockStorage(queueLock_);
        // Original clears its singleton even when a newer object replaced it.
        Current=nullptr;
    }
    const spRTTIRecord& spNetworkManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,"spNetworkManager",&spBaseObject::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spNetworkManager>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spNetworkManager::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spNetworkManager>();manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void spNetworkManager::vfunc_0C(const void* n) noexcept
    {if(state_.forwardNotifications)host_->ForwardNotification(*this,n);}
    bool spNetworkManager::StartForAnalysis()
    {
        if(state_.started)return false;
        using Service=spNetworkManagerServiceForAnalysis;
        auto create=[&](Service kind){auto h=host_->CreateService(kind,*this);if(!h)throw std::logic_error("NetworkManager foreign factory returned null");return h;};
        debug_=create(Service::Debug);host_->StartDebug(debug_);host_->SetDebugFile(debug_,1,"SparkNet.txt");
        server_=create(Service::Server);host_->SetServerQueue(server_,*this);
        peer_=create(Service::Peer);host_->SetPeerId(peer_,0);
        controller_=create(Service::Controller);statistics_.emplace();statistics_->fill(0);
        timer_=create(Service::Timer);errorLock_=host_->CreateErrorLock();
        matchmaking_=create(Service::Matchmaking);ClearError();state_.started=1;return true;
    }
    void spNetworkManager::StopForAnalysis() noexcept
    {
        using Service=spNetworkManagerServiceForAnalysis;
        auto drop=[&](Service kind,Handle& h){if(h){host_->DeleteService(kind,h);h=0;}};
        drop(Service::Server,server_);
        while(PopPacket()){} // Native removes nodes without destroying these packets.
        drop(Service::Peer,peer_);drop(Service::Controller,controller_);statistics_.reset();
        drop(Service::Timer,timer_);
        if(errorLock_){host_->FreeLockStorage(errorLock_);errorLock_=0;}
        if(matchmaking_){drop(Service::Matchmaking,matchmaking_);state_.word15=0;}
        state_.forwardNotifications=0;state_.uniqueId=0;drop(Service::Debug,debug_);state_.started=0;
    }
    void spNetworkManager::ClearError()
    {
        if(errorLock_){const Lock lock(*host_,errorLock_);state_.error=0;}else state_.error=0;
    }
    void spNetworkManager::SetErrorForAnalysis(std::int32_t error)
    {
        if(errorLock_){const Lock lock(*host_,errorLock_);state_.error=std::uint32_t(error);}else state_.error=std::uint32_t(error);
        if(!error)return;
        // Original signed indexing accepts negatives and reads outside its ten
        // strings. Reject that undefined portable domain after the native store.
        if(error<0)throw std::out_of_range("Negative NetworkManager error indexes outside its table");
        spNetworkManagerNotificationForAnalysis n;n.code=0x23;n.word0C=1;n.sender=this;n.word18=std::uint32_t(error);
        static const std::string unknown="Unknown Network Error";
        const auto& text=error<10?errors_[std::size_t(error)]:unknown;
        n.word1C=reinterpret_cast<std::uintptr_t>(text.c_str());host_->DispatchError(*this,n,text);
    }
    void spNetworkManager::Log(std::int32_t level,const std::string& format,const std::vector<std::uint32_t>& args)
    {
        // The original repeatedly resolves/lazily creates the current global
        // manager rather than using this object's debug pointer.
        auto& current=GetOrCreateForAnalysis();
        if(current.debug_)current.host_->Log(current.debug_,"Network Manager",level,format,args);
    }
    void spNetworkManager::TransitionForAnalysis(std::uint32_t a,std::uint32_t b)
    {if(!controller_)throw std::logic_error("NetworkManager controller is missing");host_->Transition(controller_,a,b);}
    void spNetworkManager::RefreshTimerForAnalysis(){host_->RefreshTimer(timer_);}
    std::uint32_t spNetworkManager::TimerMilliseconds()
    {
        auto t=host_->ReadTimer(timer_);
        if(t.active){host_->StartTimer(timer_);host_->RefreshTimer(timer_);t=host_->ReadTimer(timer_);}
        if(!t.divisor)throw std::logic_error("NetworkManager timer divisor is zero");
        return t.ticks/t.divisor;
    }
    void spNetworkManager::Increment(std::size_t index)
    {if(!statistics_)throw std::logic_error("NetworkManager statistics are missing");++(*statistics_)[index];}
    bool spNetworkManager::StartConnectionForAnalysis()
    {
        ClearError();Log(1,"Creating listening server");
        if(!host_->OpenServer(server_))
        {Log(0,"Creation failed");TransitionForAnalysis(6,1);SetErrorForAnalysis(3);return false;}
        if(!host_->ConnectPeer(peer_,0))
        {Log(0,"Connections with DGE failed");TransitionForAnalysis(6,1);SetErrorForAnalysis(2);return false;}
        host_->SetServerPeer(server_,peer_);
        if(!SendProbeForAnalysis()){Log(0,"Unable to request DGE configuration");TransitionForAnalysis(6,1);return false;}
        return true;
    }
    bool spNetworkManager::SendProbeForAnalysis()
    {
        if(host_->SendPeer(peer_,host_->ServerId(server_),0,{0x2a},1,0,0,0)<=0)
        {Increment(7);SetErrorForAnalysis(4);return false;}
        Increment(0);return true;
    }
    bool spNetworkManager::SendHelloRequestForAnalysis()
    {
        Log(0,"Sending Hello Request to DGE");const auto time=TimerMilliseconds();
        std::vector<std::uint8_t> data{std::uint8_t(time),std::uint8_t(time>>8),std::uint8_t(time>>16),std::uint8_t(time>>24),0,0,0,0};
        if(host_->SendPeer(peer_,host_->ServerId(server_),5,data,1,0,0,0)<=0)
        {Increment(12);SetErrorForAnalysis(4);return false;}
        Increment(5);return true;
    }
    const spNetworkManagerPacketForAnalysis& spNetworkManager::Packet()
    {if(!currentPacket_)throw std::logic_error("NetworkManager current packet is missing");return host_->ReadPacket(currentPacket_);}
    bool spNetworkManager::HandleIdReplyForAnalysis()
    {
        const auto& p=Packet();
        if(p.data.size()<4){Log(0,"Invalid packet size, could not contain spPktStructGameStart data");Increment(25);return false;}
        Increment(18);
        if(p.data[0]==1){ClearError();state_.uniqueId=std::uint16_t(p.data[2]|(std::uint16_t(p.data[3])<<8));TransitionForAnalysis(7,0);}
        return true;
    }
    bool spNetworkManager::HandleHelloReplyForAnalysis()
    {
        const auto& p=Packet();const auto first=Word(p.data,0),second=Word(p.data,4);
        if(!first||!second){Log(0,"Invalid Hello Reply received (%d, %d)",{second,first});Increment(27);return false;}
        const auto sample=(TimerMilliseconds()-first)>>1;
        LatencySamples[NextLatencySample]=sample;NextLatencySample=std::uint8_t((NextLatencySample+1)%5);
        std::uint32_t sum=0,count=0;for(auto v:LatencySamples)if(v){sum+=v;++count;}
        state_.bias=count?std::uint32_t(Signed(sum)/count):0;
        host_->SetPeerTimestamp(peer_,TimerMilliseconds());return true;
    }
    bool spNetworkManager::BroadcastPacketForAnalysis()
    {
        const auto& p=Packet();
        for(auto child:host_->Children(*this))
            if(host_->ChildClassId(child)==p.field1&&host_->ChildField2C(child)==p.field2)
            {spNetworkManagerNotificationForAnalysis n;n.code=0x1f;n.sender=this;n.word18=p.field1;n.word1C=p.field2;host_->Notify(child,n);}
        return true;
    }
    spNetworkManager::Handle spNetworkManager::PopPacket()
    {
        const Lock lock(*host_,queueLock_);if(queue_.empty())return 0;
        const auto p=queue_.front();queue_.pop_front();return p;
    }
    bool spNetworkManager::PumpForAnalysis()
    {
        if(!state_.forwardNotifications)return true;
        host_->PumpController(controller_);
        std::uint32_t phase;
        {const Lock lock(*host_,host_->ControllerLock(controller_));phase=host_->ControllerState(controller_);}
        if(phase==3&&TimerMilliseconds()>std::uint32_t((*statistics_)[5]*5000u))SendHelloRequestForAnalysis();
        host_->PumpMatchmaking(matchmaking_);
        for(unsigned count=0;count<25;++count)
        {
            currentPacket_=PopPacket();if(!currentPacket_)break;
            switch(Packet().type)
            {
            case 2:Increment(16);BroadcastPacketForAnalysis();break;
            case 4:Log(0,"Receive GameStart Packet");Increment(18);HandleIdReplyForAnalysis();break;
            case 6:Log(2,"Receive eptHelloReply packet");Increment(20);HandleHelloReplyForAnalysis();break;
            default:Log(0,"Got an unknown packet in the queue");break;
            }
            host_->DeletePacket(currentPacket_);currentPacket_=0;
        }
        return true;
    }
    void spNetworkManager::SetRecipientForAnalysis(Handle recipient)
    {
        spNetworkManagerNotificationForAnalysis n;n.code=0x17;n.sender=this;
        if(recipient_)host_->Notify(recipient_,n);
        recipient_=recipient;n.code=0x16;if(recipient_)host_->Notify(recipient_,n);
    }
    bool spNetworkManager::DispatchInputForAnalysis()
    {
        if(!matchmaking_||!recipient_)return true;
        const bool alternate=host_->InputPressed(matchmaking_,0x28)||host_->InputPressed(matchmaking_,0x34);
        const auto& map=alternate?sparkplug::evidence::pc::NetworkManagerAlternateKeys:sparkplug::evidence::pc::NetworkManagerDefaultKeys;
        for(std::uint32_t key=0;key<100;++key)if(host_->InputChanged(matchmaking_,key))
        {
            const auto mapped=map[key];const bool pressed=host_->InputPressed(matchmaking_,key);
            spNetworkManagerNotificationForAnalysis n;n.sender=this;
            if(pressed){n.code=mapped?0x13:0x14;n.word18=mapped?std::uint32_t(std::int32_t(mapped<128?mapped:int(mapped)-256)):key;}
            else {if(mapped)continue;n.code=0x15;n.word18=key;}
            host_->Notify(recipient_,n);
        }
        return true;
    }
    void spNetworkManager::EnqueueForAnalysis(Handle p){const Lock lock(*host_,queueLock_);queue_.push_back(p);}
    void spNetworkManager::SetControlsForAnalysis(std::uint8_t a,std::uint8_t b,std::uint8_t c) noexcept
    {state_.started=a;state_.word15=b;state_.forwardNotifications=c;}
    void spNetworkManager::SetStatisticForAnalysis(std::size_t i,std::uint32_t v)
    {if(!statistics_||i>=28)throw std::out_of_range("NetworkManager statistic is unavailable");(*statistics_)[i]=v;}
    void spNetworkManager::SetLatencySamplesForAnalysis(const std::array<std::uint32_t,5>& v,std::uint8_t next)
    {if(next>=5)throw std::out_of_range("NetworkManager latency index exceeds its five samples");LatencySamples=v;NextLatencySample=next;}
    spNetworkManager::StateForAnalysis spNetworkManager::GetStateForAnalysis() const
    {
        auto s=state_;s.queued=queue_.size();s.services={server_!=0,peer_!=0,matchmaking_!=0,debug_!=0,controller_!=0,bool(statistics_),errorLock_!=0,timer_!=0};
        if(statistics_)s.statistics.assign(statistics_->begin(),statistics_->end());return s;
    }
}
