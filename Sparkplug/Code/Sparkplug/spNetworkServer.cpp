#include "spNetworkServer.h"
#include "../../Analysis/PC/spNetworkServerAbi.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkServerHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNetworkServer>();}
        const spRTTIRecord Record{spNetworkServer::ClassID,spBaseObject::ClassID,
            "spNetworkServer",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        class Lock final
        {
        public:
            Lock(spNetworkServerHost& h,std::uintptr_t value):host(h),handle(value){host.EnterLock(handle);}
            ~Lock(){host.LeaveLock(handle);}
        private:spNetworkServerHost& host;std::uintptr_t handle;
        };
    }
    void SetNetworkServerFactoryHostForAnalysis(std::shared_ptr<spNetworkServerHost> h){FactoryHost=std::move(h);}
    std::shared_ptr<spNetworkServerHost> GetNetworkServerFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkServer::spNetworkServer(std::shared_ptr<spNetworkServerHost> h):host_(std::move(h))
    {
        if(!host_)throw std::invalid_argument("NetworkServer requires a foreign-service host");
        lock_=host_->CreatePeerLock();
        if(!lock_)throw std::runtime_error("NetworkServer peer lock is unavailable");
        try
        {
            thread_=host_->CreateThread();
            if(!thread_)throw std::runtime_error("NetworkServer thread object is unavailable");
            tcp_=host_->CreateSocketStream();
            if(!tcp_)throw std::runtime_error("NetworkServer TCP stream is unavailable");
            tcp_->SetEndpointForAnalysis(1,tcp_->AddressForAnalysis(),static_cast<std::uint16_t>(tcp_->PortWordForAnalysis()),tcp_->BlockingForAnalysis());
            udp_=host_->CreateSocketStream();
            if(!udp_)throw std::runtime_error("NetworkServer UDP stream is unavailable");
            udp_->SetEndpointForAnalysis(0,udp_->AddressForAnalysis(),static_cast<std::uint16_t>(udp_->PortWordForAnalysis()),udp_->BlockingForAnalysis());
            buffer_=std::make_unique<spMemoryStream>();(void)buffer_->Open("Packet buffer");
        }
        catch(...)
        {
            // Portable exception cleanup, not an allocation-failure claim about
            // native constructor/EH. Successful foreign factories are required
            // by the qualified native domain.
            if(thread_)host_->DeleteThread(thread_);host_->FreeLockStorage(lock_);throw;
        }
    }
    spNetworkServer::~spNetworkServer()
    {
        if(thread_)
        {
            if(host_->ThreadIsRunning(thread_))
            {
                host_->SetThreadStopByte(thread_,1);Log(1,"Soft killing listening thread , waiting");
                (void)host_->ThreadWait(thread_,1000);Log(1,"Ok it is gone");
            }
            host_->DeleteThread(thread_);thread_=0;
        }
        (void)CloseForAnalysis();
        peers_.clear();host_->FreeLockStorage(lock_);lock_=0;
        udp_.reset();tcp_.reset();(void)buffer_->Close();buffer_.reset();
        acceptedCount_=0;availableIds_.clear();
        // Native leaves queue+38 alone. Its replacement lifetime is explicit.
    }
    const spRTTIRecord& spNetworkServer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetworkServer::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkServer::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<spNetworkServer>();manager.RegisterCloneForAnalysis(*this,*result);
        if(!vfunc_14(*result,manager))return nullptr;return result;
    }
    void spNetworkServer::Log(std::int32_t level,const char* format,const std::vector<std::uint32_t>& args)
    {if(const auto debug=host_->ResolveNetworkDebug())host_->Log(debug,level,"Network Server",format,args);}
    bool spNetworkServer::OpenForAnalysis()
    {
        if(id_==0)
        {
            if(!tcp_->Open(1,nullptr))return false;Log(1,"TCP Socket now listening");
        }
        if(!udp_->Open(1,nullptr))return false;Log(1,"UDP Socket now open");
        if(!host_->ThreadCreate(thread_,evidence::pc::NetworkServerCallback,*this))return false;
        Log(1,"Thread created");if(!host_->ThreadResume(thread_))return false;
        // Native resolves the manager again after checking the first debug.
        if(host_->ResolveNetworkDebug())
            host_->Log(host_->ResolveNetworkDebug(),1,"Network Server","Thread running",{});
        return true;
    }
    bool spNetworkServer::CloseForAnalysis()
    {
        if(id_==0&&!tcp_->Close())Log(0,"TCP listening socket cannot be properly closed");
        {
            const Lock guard(*host_,lock_);
            for(auto& entry:peers_)
            {
                auto& peer=*entry.object;
                if(peer.TcpForAnalysis().NetworkForAnalysis().IsConnectedForAnalysis())(void)peer.CloseForAnalysis();
                ReleaseIdForAnalysis(peer.GetStateForAnalysis().id);
            }
        }
        if(!udp_->Close())Log(0,"UDP listening socket cannot be properly closed");
        acceptedCount_=0;return true;
    }
    bool spNetworkServer::SetQueueForAnalysis(Handle queue)
    {
        // Same-pointer replacement also deletes the previous object in native.
        // Provider must reject invalid reuse rather than invent its lifetime.
        if(queue_)host_->DeleteQueue(queue_);queue_=queue;return true;
    }
    bool spNetworkServer::SetPeerForAnalysis(spNetworkPeer& peer)
    {
        if(peer.GetStateForAnalysis().byte12)throw std::invalid_argument("Accepted peer requires an explicit ownership transfer");
        const Lock guard(*host_,lock_);peers_.push_back({&peer,{}});return true;
    }
    void spNetworkServer::AdoptPeerForAnalysis(std::unique_ptr<spNetworkPeer> peer)
    {
        if(!peer||!peer->GetStateForAnalysis().byte12)throw std::invalid_argument("Native accepted-peer ownership flag is missing");
        const Lock guard(*host_,lock_);auto* object=peer.get();peers_.push_back({object,std::move(peer)});
    }
    bool spNetworkServer::FindPeerForAnalysis(spNetworkPeer*& result,std::uint16_t id)
    {
        if(result)return true;
        if(id==0xbad0){Log(0,"Searching for invalid source ID");return false;}
        {
            const Lock guard(*host_,lock_);
            for(auto& entry:peers_)
            {
                const auto state=entry.object->GetStateForAnalysis();
                if(state.id==id&&state.byte12)result=entry.object; // Last match wins.
            }
        }
        if(result)return true;Log(0,"Cannot find associated spNetworkPeer");return false;
    }
    std::uint32_t spNetworkServer::CountOwnedPeersForAnalysis()
    {
        const Lock guard(*host_,lock_);std::uint32_t count=0;
        for(auto& entry:peers_)if(entry.object->GetStateForAnalysis().byte12)++count;
        return count;
    }
    void spNetworkServer::PruneDisconnectedForAnalysis()
    {
        const Lock guard(*host_,lock_);
        for(auto it=peers_.begin();it!=peers_.end();)
        {
            auto& peer=*it->object;
            if(!peer.TcpForAnalysis().NetworkForAnalysis().IsConnectedForAnalysis())
            {
                if(!peer.GetStateForAnalysis().byte12)
                {host_->ManagerError(4);host_->ManagerTransition(8,0);}
                else
                {
                    host_->ManagerError(6);it=peers_.erase(it);
                    if(acceptedCount_)--acceptedCount_;
                    // Original advances to the new next node and then advances
                    // again in the common loop tail, skipping one neighbour.
                    if(it!=peers_.end())++it;
                    continue;
                }
            }
            ++it;
        }
    }
    bool spNetworkServer::RemovePeerForAnalysis(spNetworkPeer& peer)
    {
        const Lock guard(*host_,lock_);
        for(auto& entry:peers_)if(entry.object==&peer)
        {ReleaseIdForAnalysis(peer.GetStateForAnalysis().id);(void)peer.CloseForAnalysis();return true;}
        return false; // Native retains the matching list node and count.
    }
    bool spNetworkServer::BroadcastForAnalysis(spNetworkPacket& packet)
    {
        const Lock guard(*host_,lock_);
        for(auto& entry:peers_)
        {
            auto& peer=*entry.object;const auto state=peer.GetStateForAnalysis();
            if(state.byte12&&packet.HeaderForAnalysis().source!=state.id)(void)peer.SendPacketForAnalysis(packet,1);
        }
        return true;
    }
    void spNetworkServer::ReleaseIdForAnalysis(std::uint16_t id)
    {
        if(id_!=0)return;
        if(!maximumId_)throw std::logic_error("NetworkServer maximum ID is unwritten");
        if(!id||id>=*maximumId_){Log(0,"Trying to release invalid ID");return;}
        availableIds_.push_front(id);availableIds_.unique(); // Adjacent duplicates only; no sort.
    }
    std::uint16_t spNetworkServer::AllocateIdForAnalysis()
    {
        if(availableIds_.empty())return 0xbad0;
        const auto id=availableIds_.front();availableIds_.pop_front();return id;
    }
    bool spNetworkServer::HandleInitialRequestForAnalysis(spNetworkPeer& peer,const spNetworkPacket& packet)
    {
        if(host_->ReadManagerControllerState()!=1&&host_->ReadManagerControllerState()!=2)
        {
            Log(0,"Should not have receive InitialInfoRequest message in this state");
            host_->IncrementManagerStatistic(21);return false;
        }
        auto id=peer.GetStateForAnalysis().id;
        if(packet.HeaderForAnalysis().source==0){id=0;peer.SetIdForAnalysis(id);}
        else
        {
            if(id==0xbad0){id=AllocateIdForAnalysis();peer.SetIdForAnalysis(id);}
            else Log(0,"The peer is asking for its Id again: %d",{id});
            Log(1,"Informing peer of its ID: %d",{id});
        }
        const std::array<std::uint8_t,2> payload{std::uint8_t(id),std::uint8_t(id>>8)};
        host_->IncrementManagerStatistic(15);
        if(peer.SendForAnalysis(id_,1,payload.data(),2,1,1,0,0)>0)return true;
        host_->ManagerError(8);return false;
    }
    bool spNetworkServer::HandleInitialReplyForAnalysis(const spNetworkPacket& packet)
    {
        const auto* data=packet.KnownPayloadForAnalysis(2);
        const auto id=std::uint16_t(data[0]|(std::uint16_t(data[1])<<8));
        if(id==0xbad0){host_->ManagerTransition(6,1);Log(0,"Invalid unique ID received (%04X)",{id});}
        id_=id;host_->ManagerTransition(5,0);Log(1,"Unique ID is now %d",{id_});return true;
    }
    bool spNetworkServer::HandleReadyForAnalysis(spNetworkPeer* peer,const spNetworkPacket& packet)
    {
        const auto header=packet.HeaderForAnalysis();
        if(!FindPeerForAnalysis(peer,header.source))return false;
        const auto* data=packet.KnownPayloadForAnalysis(1);
        if(!data[0])
        {Log(1,"GE %d is not ready",{header.source});peer->SetReadyByteForAnalysis(0);return true;}
        if(header.source>CountOwnedPeersForAnalysis()||header.destination!=0)
        {
            Log(1,"GE said to be ready: error in received data (Src ID: %d, Dst. ID: %d )",{header.source,header.destination});
            peer->SetReadyByteForAnalysis(0);return false;
        }
        if(host_->ResolveNetworkDebug())
            host_->Log(host_->ResolveNetworkDebug(),1,"Network Server","GE %d is ready",{header.source});
        peer->SetReadyByteForAnalysis(1);return true;
    }
    std::uint32_t spNetworkServer::TimerMilliseconds()
    {
        auto timer=host_->ReadManagerTimer();
        if(timer.active){host_->StartManagerTimer();host_->RefreshManagerTimer();timer=host_->ReadManagerTimer();}
        if(!timer.divisor)throw std::logic_error("NetworkServer timer divisor is zero");return timer.ticks/timer.divisor;
    }
    bool spNetworkServer::HandleHelloForAnalysis(spNetworkPeer* peer,spNetworkPacket& packet)
    {
        if(!FindPeerForAnalysis(peer,packet.HeaderForAnalysis().source))return false;
        peer->SetTimestampForAnalysis(TimerMilliseconds());
        const auto* original=packet.KnownPayloadForAnalysis(8);
        std::array<std::uint8_t,8> payload{};std::copy_n(original,8,payload.begin());
        const auto now=TimerMilliseconds();for(unsigned i=0;i<4;++i)payload[4+i]=std::uint8_t(now>>(8*i));
        packet.SetPayloadForAnalysis(payload.data(),payload.size());
        const auto sent=peer->SendForAnalysis(id_,6,payload.data(),8,1,0,0,0);
        if(sent>0)return true;
        host_->ManagerError(8);return false;
    }
    bool spNetworkServer::ProcessPacketsForAnalysis(spNetworkPeer* peer,spStream& stream)
    {
        (void)stream.Seek(spStream::SeekSource::essStart,0);
        for(;;)
        {
            auto packet=host_->CreatePacket();
            if(!packet)throw std::runtime_error("NetworkServer packet factory returned null");
            if(!packet->ReadForAnalysis(stream))return true;
            Log(2,"Receive new packet at server");
            host_->DumpPacket(host_->ResolveNetworkDebug(),*packet);
            const auto header=packet->HeaderForAnalysis();
            if(header.destination!=id_&&header.destination!=0xffff)
            {Log(0,"Receive unknown packet destination");continue;}
            if(!header.packetType)throw std::logic_error("NetworkServer packet type is unwritten");
            switch(*header.packetType)
            {
            case 0:
                Log(1,"Receive eptInitialInfoRequest packet");host_->IncrementManagerStatistic(14);
                if(!peer)throw std::invalid_argument("InitialInfoRequest requires its native peer receiver");
                (void)HandleInitialRequestForAnalysis(*peer,*packet);continue;
            case 1:
                Log(1,"Receive eptInitialInfoReply packet");host_->IncrementManagerStatistic(15);
                (void)HandleInitialReplyForAnalysis(*packet);continue;
            case 3:
                Log(1,"Receive eptReadyToStart packet");host_->IncrementManagerStatistic(17);
                (void)HandleReadyForAnalysis(peer,*packet);continue;
            case 5:
                Log(2,"Receive eptHelloRequest packet");host_->IncrementManagerStatistic(19);
                (void)HandleHelloForAnalysis(peer,*packet);continue;
            default:break;
            }
            if(header.destination==0xffff&&id_==0)
            {Log(2,"Receive broadcast packet");(void)BroadcastForAnalysis(*packet);continue;}
            if(header.destination==id_)
            {
                Log(2,"Puting packet in Network Manager queue");
                if(queue_)host_->EnqueuePacket(queue_,std::move(packet));
                else host_->RetainUnqueuedPacket(std::move(packet));
                continue;
            }
            Log(0,"Receive packet to unknown destination");
        }
    }
    bool spNetworkServer::HandleNewTcpConnectionForAnalysis(std::unique_ptr<spSocketStream>& incoming)
    {
        if(!incoming)throw std::invalid_argument("NetworkServer admission requires the accepted stream");
        if(!maximumId_)throw std::logic_error("NetworkServer maximum is unwritten");
        // This is the fixed original sprintf rendered after CRT formatting.
        // Signed %d is retained even though the capacity comparison is unsigned.
        const auto signedCount=acceptedCount_<0x80000000u?std::int64_t(acceptedCount_):std::int64_t(acceptedCount_)-0x100000000LL;
        const auto diagnostic=std::string("HandleNewTcpConnection: Number Players: ")+std::to_string(signedCount)+"  Max Players: "+std::to_string(*maximumId_);
        if(const auto debug=host_->ResolveNetworkDebug())
            host_->Log(debug,host_->AdmissionLogLevel(),"Network Server",diagnostic,{});
        if(acceptedCount_>=*maximumId_)
        {Log(1,"Already have max players - New TCP Connection attempt REJECTED");return false;}
        auto peer=host_->CreatePeer();
        if(!peer)throw std::runtime_error("NetworkServer peer factory returned null");
        if(!peer->AdoptTcpForAnalysis(std::move(incoming),static_cast<std::uint16_t>(udp_->PortWordForAnalysis())))
        {
            Log(0,"Problem while opening spNetworkPeer");
            host_->RetainUnacceptedPeer(std::move(peer));return false;
        }
        AdoptPeerForAnalysis(std::move(peer));++acceptedCount_;return true;
    }
    void spNetworkServer::RunWorkerForAnalysis()
    {
        while(!host_->ReadThreadStopByte(thread_))
        {
            auto& udpNetwork=udp_->NetworkForAnalysis();
            if(!udpNetwork.ClearSetForAnalysis(0)&&!udpNetwork.ClearSetForAnalysis(1))
                (void)udpNetwork.ClearSetForAnalysis(2);
            if(id_==0)
            {
                auto incoming=tcp_->AcceptForAnalysis();
                if(incoming&&!HandleNewTcpConnectionForAnalysis(incoming))
                {
                    if(!incoming)throw std::runtime_error("Native worker would delete the stream retained by a failed adopted Peer");
                    (void)incoming->Close();incoming.reset();
                }
                auto& listening=tcp_->NetworkForAnalysis();
                (void)listening.AddToSetForAnalysis(0);(void)listening.AddToSetForAnalysis(2);
            }
            (void)buffer_->Reset();
            if(udp_->ReceiveStreamForAnalysis(*buffer_))
            {
                std::uint32_t ignored=0;(void)buffer_->GetSize(&ignored);
                (void)ProcessPacketsForAnalysis(nullptr,*buffer_);
            }
            (void)udp_->NetworkForAnalysis().AddToSetForAnalysis(0);
            (void)udp_->NetworkForAnalysis().AddToSetForAnalysis(2);
            {
                const Lock guard(*host_,lock_);
                for(auto& entry:peers_)
                {
                    auto& peer=*entry.object;
                    if(std::uint32_t(TimerMilliseconds()-peer.GetStateForAnalysis().timestamp)>40000u&&
                        peer.TcpForAnalysis().NetworkForAnalysis().IsConnectedForAnalysis())
                    {
                        Log(0,"Closing connection since no hello received for a while");
                        (void)RemovePeerForAnalysis(peer);continue;
                    }
                    (void)buffer_->Reset();
                    if(!peer.TcpForAnalysis().ReceiveStreamForAnalysis(*buffer_))continue;
                    std::uint32_t size=0;(void)buffer_->GetSize(&size);
                    if(!size)
                    {Log(1,"0 byte received, connection closed");(void)RemovePeerForAnalysis(peer);}
                    else (void)ProcessPacketsForAnalysis(&peer,*buffer_);
                    auto& network=peer.TcpForAnalysis().NetworkForAnalysis();
                    if(network.IsConnectedForAnalysis())
                    {(void)network.AddToSetForAnalysis(0);(void)network.AddToSetForAnalysis(2);}
                }
            }
            PruneDisconnectedForAnalysis();
            (void)udp_->NetworkForAnalysis().SelectForAnalysis(1000);
        }
    }
    spNetworkServer::StateForAnalysis spNetworkServer::GetStateForAnalysis() const
    {
        StateForAnalysis state{id_,maximumId_,acceptedCount_,{},{},queue_!=0};
        for(const auto& entry:peers_)state.peerIds.push_back(entry.object->GetStateForAnalysis().id);
        state.availableIds.assign(availableIds_.begin(),availableIds_.end());return state;
    }
    void spNetworkServer::SetStateForAnalysis(std::uint16_t id,std::uint16_t maximumId,std::uint32_t count) noexcept
    {id_=id;maximumId_=maximumId;acceptedCount_=count;}
    std::vector<spNetworkServer::PeerStateForAnalysis> spNetworkServer::GetPeerStatesForAnalysis() const
    {
        std::vector<PeerStateForAnalysis> result;
        for(const auto& entry:peers_)
        {
            const auto state=entry.object->GetStateForAnalysis();
            result.push_back({state.id,state.unknown13,state.timestamp,
                entry.object->TcpForAnalysis().NetworkForAnalysis().IsConnectedForAnalysis()!=0});
        }
        return result;
    }
}
