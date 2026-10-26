// Component-graph integration only. Literal field producers and foreign
// OS/network/thread/debug/timer/matchmaking services are explicit fixtures;
// no game startup, real socket, native thread or playable session is claimed.
#include "Code/Sparkplug/spNetworkManager.h"
#include "Code/Sparkplug/spNetworkServer.h"
#include "Code/Sparkplug/spNetworkStateCtrl.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
    struct Graph;
    class Network final:public spNetwork
    {
    public:
        Graph& graph;unsigned identity;
        Network(Graph&,unsigned);
        ~Network() override;
        bool OpenForAnalysis(std::uint32_t) override;
        bool ConnectForAnalysis(std::uint32_t,std::uint16_t) override;
        bool BindForAnalysis(std::uint32_t,std::uint16_t) override;
        bool ListenForAnalysis() override{return true;}
        bool CloseForAnalysis() override{connected_=0;return true;}
        bool SetBlockingForAnalysis(std::uint8_t) override{return true;}
        int SendForAnalysis(const void*,int) override;
        spNetworkAcceptForAnalysis AcceptForAnalysis() override{throw std::logic_error("Unexpected accept in handshake graph");}
        bool AddToSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected socket-set call");}
        bool RemoveFromSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected socket-set call");}
        bool ClearSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected socket-set call");}
        bool SelectForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected select");}
        int SendToForAnalysis(std::uint32_t,std::uint16_t,const void*,int) override{throw std::logic_error("Unexpected sendto");}
        int ReceiveForAnalysis(void*,int) override{throw std::logic_error("Stream input is injected through actual MemoryStream");}
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void*,int) override{throw std::logic_error("Unexpected recvfrom");}
        bool GetLocalAddressForAnalysis(std::uint32_t&) override{throw std::logic_error("Unexpected address lookup");}
    };
    struct Bridge final:spNetworkManagerHost,spNetworkServerHost,spNetworkPeerHost,
        spNetworkStateCtrlHost,spSocketStreamHost,std::enable_shared_from_this<Bridge>
    {
        using Handle=std::uintptr_t;
        using Service=spNetworkManagerServiceForAnalysis;
        Graph& graph;
        explicit Bridge(Graph& value):graph(value){}
        Handle CreateQueueLock() override;
        Handle CreateErrorLock() override;
        Handle CreatePeerLock() override;
        Handle CreateLock() override;
        void FreeLockStorage(Handle) noexcept override;
        void EnterLock(Handle) override;
        void LeaveLock(Handle) noexcept override;
        Handle CreateService(Service,spNetworkManager&) override;
        void DeleteService(Service,Handle) noexcept override;
        void StartDebug(Handle) override;
        void SetDebugFile(Handle,std::uint8_t,const std::string&) override;
        void SetServerQueue(Handle,spNetworkManager&) override;
        void SetPeerId(Handle,std::uint16_t) override;
        void Transition(Handle,std::uint32_t,std::uint32_t) override;
        void PumpController(Handle) override;
        Handle ControllerLock(Handle) override;
        std::uint32_t ControllerState(Handle) override;
        void PumpMatchmaking(Handle) override;
        spNetworkManagerHost::Timer ReadTimer(Handle) override{return {0,0,1};}
        void StartTimer(Handle) override;
        void RefreshTimer(Handle) override;
        void SetPeerTimestamp(Handle,std::uint32_t) override;
        std::uint16_t ServerId(Handle) override;
        bool OpenServer(Handle) override;
        bool ConnectPeer(Handle,std::uint32_t) override;
        void SetServerPeer(Handle,Handle) override;
        std::int32_t SendPeer(Handle,std::uint16_t,std::uint16_t,const std::vector<std::uint8_t>&,
            std::uint8_t,std::uint32_t,std::uint32_t,std::uint32_t) override;
        const spNetworkManagerPacketForAnalysis& ReadPacket(Handle) override;
        void DeletePacket(Handle) noexcept override;
        void ForwardNotification(spNetworkManager&,const void*) override{throw std::logic_error("Unexpected root notification forwarding");}
        void DispatchError(spNetworkManager&,const spNetworkManagerNotificationForAnalysis&,const std::string&) override
        {throw std::logic_error("Unexpected network error in graph flow");}
        void Log(Handle,const std::string&,std::int32_t,const std::string&,const std::vector<std::uint32_t>&) override;
        std::vector<Handle> NotificationOwners(spNetworkManager&) override{return {0x100,0x101,0x102};}
        std::uint32_t OwnerClassId(Handle owner) override{return owner==0x102?0x7777:0xaced;}
        std::uint32_t OwnerField2C(Handle owner) override{return owner==0x101?0x9999:0x5432;}
        void Notify(Handle,const spNetworkManagerNotificationForAnalysis&) override;
        Handle CreateThread() override{return 0x30;}
        void DeleteThread(Handle) noexcept override;
        bool ThreadCreate(Handle,std::uint32_t,spNetworkServer&) override;
        bool ThreadResume(Handle) override;
        bool ThreadIsRunning(Handle) override{return false;}
        void SetThreadStopByte(Handle,std::uint8_t) override{throw std::logic_error("No native worker is running");}
        bool ThreadWait(Handle,std::uint32_t) override{throw std::logic_error("No native worker is running");}
        std::uint8_t ReadThreadStopByte(Handle) override{throw std::logic_error("Worker loop is outside this test");}
        std::unique_ptr<spSocketStream> CreateSocketStream() override
        {return std::make_unique<spSocketStream>(shared_from_this());}
        Handle ResolveNetworkDebug() override;
        std::int32_t AdmissionLogLevel() override{throw std::logic_error("Admission debug is outside this flow");}
        void Log(Handle,std::int32_t,const std::string&,const std::string&,const std::vector<std::uint32_t>&) override;
        void Log(Handle,std::int32_t,const std::string&,const std::string&) override
        {throw std::logic_error("Peer debug is explicitly absent");}
        void ManagerError(std::uint32_t) override{throw std::logic_error("Unexpected Server manager error");}
        void ManagerTransition(std::uint32_t,std::uint8_t) override;
        spNetworkServerHost::Timer ReadManagerTimer() override{return {0,0,1};}
        void StartManagerTimer() override{throw std::logic_error("Timer active byte is explicitly zero");}
        void RefreshManagerTimer() override{throw std::logic_error("Timer active byte is explicitly zero");}
        std::uint32_t ReadManagerControllerState() override;
        void IncrementManagerStatistic(std::size_t) override;
        std::unique_ptr<spNetworkPacket> CreatePacket() override;
        void DumpPacket(Handle,const spNetworkPacket&) override;
        void EnqueuePacket(Handle,std::unique_ptr<spNetworkPacket>) override;
        void RetainUnqueuedPacket(std::unique_ptr<spNetworkPacket>) override;
        std::unique_ptr<spNetworkPeer> CreatePeer() override{return std::make_unique<spNetworkPeer>(shared_from_this());}
        void RetainUnacceptedPeer(std::unique_ptr<spNetworkPeer>) override;
        void DeleteQueue(Handle) override{throw std::logic_error("Only initial queue assignment is exercised");}
        Handle ResolveNetworkManager() override;
        bool StartConnection(Handle) override;
        void ManagerOperation450B70(Handle) override;
        void ManagerOperation4516F0(Handle,std::uint32_t) override;
        std::unique_ptr<spNetwork> CreateNetwork() override;
        void ReportInvalidNetwork() override{throw std::logic_error("Unexpected invalid network adoption");}
    };
    struct Graph
    {
        using Handle=Bridge::Handle;
        static constexpr Handle Debug=0x10,Timer=0x20,Matchmaking=0x40,Queue=0x50;
        struct PacketOwner
        {
            std::unique_ptr<spNetworkPacket> packet;
            spNetworkManagerPacketForAnalysis view;
        };
        std::shared_ptr<Bridge> bridge;
        std::unique_ptr<spNetworkManager> manager;
        std::unique_ptr<spNetworkServer> server;
        std::unique_ptr<spNetworkPeer> peer;
        std::unique_ptr<spNetworkStateCtrl> controller;
        std::map<Handle,unsigned> locks;
        std::map<Handle,PacketOwner> queued;
        std::vector<std::unique_ptr<spNetworkPacket>> unqueued;
        std::vector<std::unique_ptr<spNetworkPeer>> orphans;
        std::vector<std::vector<std::uint8_t>> wire;
        std::vector<std::string> order;
        std::vector<std::uint16_t> dumped,deletedTypes;
        std::vector<Handle> readyObservers,dataObservers;
        std::vector<std::pair<std::uint32_t,std::uint8_t>> transitions;
        Handle nextLock=0x1000,controllerLock=0;
        unsigned liveNetworks=0,networksCreated=0,packetsCreated=0,timerRefreshes=0,matchPumps=0;
        bool lifetimeFailure=false,threadCreated=false,threadDeleted=false,debugLive=false;
        Graph()
        {
            bridge=std::make_shared<Bridge>(*this);
            manager=std::make_unique<spNetworkManager>(bridge);
        }
        Handle NewLock(){const auto value=nextLock++;locks.emplace(value,0);return value;}
        static Handle Address(const void* value){return reinterpret_cast<Handle>(value);}
        void Close()
        {
            manager->StopForAnalysis();manager.reset();
            Check(!server&&!peer&&!controller,"all actual Manager services destroyed");
            Check(!liveNetworks&&locks.empty()&&!lifetimeFailure,"network and lock lifetime balances");
            Check(queued.empty()&&unqueued.empty()&&orphans.empty(),"no retained packet or peer owner");
            Check(threadCreated&&threadDeleted,"thread fixture lifetime completed");
        }
    };
    Network::Network(Graph& owner,unsigned id):graph(owner),identity(id){++graph.liveNetworks;}
    Network::~Network(){--graph.liveNetworks;}
    bool Network::OpenForAnalysis(std::uint32_t type){type_=type;return true;}
    bool Network::ConnectForAnalysis(std::uint32_t address,std::uint16_t port)
    {address_=address;port_=port;connected_=1;return true;}
    bool Network::BindForAnalysis(std::uint32_t address,std::uint16_t port)
    {address_=address;port_=port;return true;}
    int Network::SendForAnalysis(const void* data,int size)
    {
        Check(connected_&&size>=17&&size<=273,"actual Peer sends a bounded connected packet");
        const auto* bytes=static_cast<const std::uint8_t*>(data);
        graph.wire.emplace_back(bytes,bytes+size);
        graph.order.push_back("wire:"+std::to_string(std::uint16_t(bytes[4]|(std::uint16_t(bytes[5])<<8))));
        return size;
    }
    Bridge::Handle Bridge::CreateQueueLock(){return graph.NewLock();}
    Bridge::Handle Bridge::CreateErrorLock(){return graph.NewLock();}
    Bridge::Handle Bridge::CreatePeerLock(){return graph.NewLock();}
    Bridge::Handle Bridge::CreateLock(){return graph.controllerLock=graph.NewLock();}
    void Bridge::FreeLockStorage(Handle value) noexcept
    {const auto it=graph.locks.find(value);if(it==graph.locks.end()||it->second)graph.lifetimeFailure=true;else graph.locks.erase(it);}
    void Bridge::EnterLock(Handle value){auto it=graph.locks.find(value);Check(it!=graph.locks.end(),"known lock");++it->second;}
    void Bridge::LeaveLock(Handle value) noexcept
    {const auto it=graph.locks.find(value);if(it==graph.locks.end()||!it->second)graph.lifetimeFailure=true;else --it->second;}
    Bridge::Handle Bridge::CreateService(Service kind,spNetworkManager& manager)
    {
        Check(&manager==graph.manager.get(),"actual Manager creates its services");
        switch(kind)
        {
        case Service::Debug:graph.debugLive=true;return Graph::Debug;
        case Service::Server:graph.server=std::make_unique<spNetworkServer>(shared_from_this());return Graph::Address(graph.server.get());
        case Service::Peer:graph.peer=std::make_unique<spNetworkPeer>(shared_from_this());return Graph::Address(graph.peer.get());
        case Service::Controller:graph.controller=std::make_unique<spNetworkStateCtrl>(shared_from_this());return Graph::Address(graph.controller.get());
        case Service::Timer:return Graph::Timer;
        case Service::Matchmaking:return Graph::Matchmaking;
        }
        throw std::logic_error("Unknown service");
    }
    void Bridge::DeleteService(Service kind,Handle value) noexcept
    {
        switch(kind)
        {
        case Service::Server:if(value!=Graph::Address(graph.server.get()))graph.lifetimeFailure=true;graph.server.reset();break;
        case Service::Peer:if(value!=Graph::Address(graph.peer.get()))graph.lifetimeFailure=true;graph.peer.reset();break;
        case Service::Controller:if(value!=Graph::Address(graph.controller.get()))graph.lifetimeFailure=true;graph.controller.reset();break;
        case Service::Debug:if(value!=Graph::Debug)graph.lifetimeFailure=true;graph.debugLive=false;break;
        case Service::Timer:if(value!=Graph::Timer)graph.lifetimeFailure=true;break;
        case Service::Matchmaking:if(value!=Graph::Matchmaking)graph.lifetimeFailure=true;break;
        }
    }
    void Bridge::StartDebug(Handle value){Check(value==Graph::Debug,"debug handle");}
    Bridge::Handle Bridge::ResolveNetworkDebug(){return graph.debugLive?Graph::Debug:0;}
    void Bridge::SetDebugFile(Handle value,std::uint8_t enabled,const std::string& name)
    {Check(value==Graph::Debug&&enabled==1&&name=="SparkNet.txt","actual Manager debug setup");}
    void Bridge::SetServerQueue(Handle value,spNetworkManager& manager)
    {Check(value==Graph::Address(graph.server.get())&& &manager==graph.manager.get(),"actual queue owner");Check(graph.server->SetQueueForAnalysis(Graph::Queue),"initial Server queue assignment");}
    void Bridge::SetPeerId(Handle value,std::uint16_t id)
    {Check(value==Graph::Address(graph.peer.get()),"actual Peer ID producer");graph.peer->SetIdForAnalysis(id);}
    void Bridge::Transition(Handle value,std::uint32_t event,std::uint32_t immediate)
    {Check(value==Graph::Address(graph.controller.get()),"actual transition receiver");graph.transitions.emplace_back(event,static_cast<std::uint8_t>(immediate));graph.controller->RequestForAnalysis(event,static_cast<std::uint8_t>(immediate));}
    void Bridge::PumpController(Handle value)
    {Check(value==Graph::Address(graph.controller.get()),"actual controller pump receiver");graph.order.push_back("controller-pump");graph.controller->PumpForAnalysis();}
    Bridge::Handle Bridge::ControllerLock(Handle value)
    {Check(value==Graph::Address(graph.controller.get()),"controller lock receiver");return graph.controllerLock;}
    std::uint32_t Bridge::ControllerState(Handle value)
    {Check(value==Graph::Address(graph.controller.get()),"controller phase receiver");return graph.controller->GetStateForAnalysis().current;}
    void Bridge::PumpMatchmaking(Handle value)
    {Check(value==Graph::Matchmaking,"matchmaking fixture");++graph.matchPumps;graph.order.push_back("matchmaking-pump");}
    void Bridge::StartTimer(Handle value){Check(value==Graph::Timer,"timer fixture");}
    void Bridge::RefreshTimer(Handle value)
    {
        // StateCtrl and Manager share this host signature with different
        // receiver types. Delegate the former to the real Manager operation.
        if(value==Graph::Address(graph.manager.get())){graph.manager->RefreshTimerForAnalysis();return;}
        Check(value==Graph::Timer,"timer fixture");++graph.timerRefreshes;graph.order.push_back("timer-refresh");
    }
    void Bridge::SetPeerTimestamp(Handle value,std::uint32_t ticks)
    {Check(value==Graph::Address(graph.peer.get()),"Peer time producer");graph.peer->SetTimestampForAnalysis(ticks);}
    std::uint16_t Bridge::ServerId(Handle value)
    {Check(value==Graph::Address(graph.server.get()),"actual Server ID");return graph.server->GetStateForAnalysis().id;}
    bool Bridge::OpenServer(Handle value)
    {Check(value==Graph::Address(graph.server.get()),"actual Server Open receiver");return graph.server->OpenForAnalysis();}
    bool Bridge::ConnectPeer(Handle value,std::uint32_t onlyTcp)
    {Check(value==Graph::Address(graph.peer.get()),"actual Peer Open receiver");return graph.peer->OpenForAnalysis(static_cast<std::uint8_t>(onlyTcp));}
    void Bridge::SetServerPeer(Handle server,Handle peer)
    {Check(server==Graph::Address(graph.server.get())&&peer==Graph::Address(graph.peer.get()),"actual Server/Peer link");Check(graph.server->SetPeerForAnalysis(*graph.peer),"native borrowed Peer link");}
    std::int32_t Bridge::SendPeer(Handle value,std::uint16_t source,std::uint16_t type,const std::vector<std::uint8_t>& bytes,
        std::uint8_t tcp,std::uint32_t destinationOverride,std::uint32_t field1,std::uint32_t field2)
    {
        Check(value==Graph::Address(graph.peer.get())&&bytes.size()<=256,"actual Peer send receiver/input");
        return graph.peer->SendForAnalysis(source,type,bytes.data(),static_cast<std::uint16_t>(bytes.size()),tcp,static_cast<std::uint8_t>(destinationOverride),field1,field2);
    }
    const spNetworkManagerPacketForAnalysis& Bridge::ReadPacket(Handle value){return graph.queued.at(value).view;}
    void Bridge::DeletePacket(Handle value) noexcept
    {
        const auto it=graph.queued.find(value);
        if(it==graph.queued.end()){graph.lifetimeFailure=true;return;}
        graph.deletedTypes.push_back(it->second.view.type);graph.queued.erase(it);
    }
    void Bridge::Log(Handle debug,const std::string& category,std::int32_t,const std::string&,const std::vector<std::uint32_t>&)
    {Check(debug==Graph::Debug&&category=="Network Manager","explicit Manager debug boundary");}
    void Bridge::Log(Handle debug,std::int32_t,const std::string& category,const std::string&,const std::vector<std::uint32_t>&)
    {Check(debug==Graph::Debug&&graph.debugLive&&category=="Network Server","current Manager supplies Server debug boundary");}
    void Bridge::Notify(Handle owner,const spNetworkManagerNotificationForAnalysis& n)
    {
        Check(n.sender==graph.manager.get()&&n.word04==0&&n.word08==0&&n.word0C==0&&n.word14==0,"actual notification fields");
        if(n.code==0x20)
        {Check(graph.controller->GetStateForAnalysis().current==2&&n.word18==0xffff&&!n.word1C,"state2 observer notification");graph.readyObservers.push_back(owner);graph.order.push_back("observer-ready");}
        else
        {Check(n.code==0x1f&&n.word18==0xaced&&n.word1C==0x5432,"generic packet notification");graph.dataObservers.push_back(owner);}
    }
    void Bridge::DeleteThread(Handle value) noexcept{if(value!=0x30||graph.threadDeleted)graph.lifetimeFailure=true;graph.threadDeleted=true;}
    bool Bridge::ThreadCreate(Handle value,std::uint32_t token,spNetworkServer& body)
    {Check(value==0x30&&token==0x484800&& &body==graph.server.get(),"foreign thread callback contract");graph.threadCreated=true;return true;}
    bool Bridge::ThreadResume(Handle value){Check(value==0x30&&graph.threadCreated,"foreign thread resume order");return true;}
    void Bridge::ManagerTransition(std::uint32_t event,std::uint8_t immediate){graph.manager->TransitionForAnalysis(event,immediate);}
    std::uint32_t Bridge::ReadManagerControllerState()
    {EnterLock(graph.controllerLock);const auto value=graph.controller->GetStateForAnalysis().current;LeaveLock(graph.controllerLock);return value;}
    void Bridge::IncrementManagerStatistic(std::size_t index)
    {const auto state=graph.manager->GetStateForAnalysis();Check(index<state.statistics.size(),"foreign Manager statistic index");graph.manager->SetStatisticForAnalysis(index,state.statistics[index]+1);}
    std::unique_ptr<spNetworkPacket> Bridge::CreatePacket(){++graph.packetsCreated;return std::make_unique<spNetworkPacket>();}
    void Bridge::DumpPacket(Handle debug,const spNetworkPacket& packet)
    {Check(debug==Graph::Debug&&graph.debugLive&&packet.HeaderForAnalysis().packetType.has_value(),"current Manager debug packet dump boundary");graph.dumped.push_back(*packet.HeaderForAnalysis().packetType);}
    void Bridge::EnqueuePacket(Handle queue,std::unique_ptr<spNetworkPacket> packet)
    {
        Check(queue==Graph::Queue&&packet!=nullptr,"Server transfers an actual packet to the Manager queue adapter");
        const auto header=packet->HeaderForAnalysis();Check(header.packetType.has_value(),"known queued packet type");
        spNetworkManagerPacketForAnalysis view;view.type=*header.packetType;view.field1=header.dataField1;view.field2=header.dataField2;
        const auto* data=packet->KnownPayloadForAnalysis(header.size);view.data.assign(data,data+header.size);
        const auto value=Graph::Address(packet.get());Check(graph.queued.emplace(value,Graph::PacketOwner{std::move(packet),std::move(view)}).second,"one packet owner");
        graph.manager->EnqueueForAnalysis(value);
    }
    void Bridge::RetainUnqueuedPacket(std::unique_ptr<spNetworkPacket> packet){graph.unqueued.push_back(std::move(packet));}
    void Bridge::RetainUnacceptedPeer(std::unique_ptr<spNetworkPeer> peer){graph.orphans.push_back(std::move(peer));}
    Bridge::Handle Bridge::ResolveNetworkManager(){Check(graph.manager!=nullptr,"current Manager exists");return Graph::Address(graph.manager.get());}
    bool Bridge::StartConnection(Handle manager)
    {Check(manager==Graph::Address(graph.manager.get())&&graph.controller->GetStateForAnalysis().current==1,"state1 entry precedes StartConnection");graph.order.push_back("start-connection");return graph.manager->StartConnectionForAnalysis();}
    void Bridge::ManagerOperation450B70(Handle manager)
    {Check(manager==Graph::Address(graph.manager.get()),"observer owner");graph.manager->NotifyObserversForAnalysis();}
    void Bridge::ManagerOperation4516F0(Handle manager,std::uint32_t ready)
    {Check(manager==Graph::Address(graph.manager.get()),"ready owner");Check(graph.manager->SendReadyForAnalysis(static_cast<std::uint8_t>(ready)),"actual Ready send");}
    std::unique_ptr<spNetwork> Bridge::CreateNetwork(){return std::make_unique<Network>(graph,++graph.networksCreated);}
    void Fill(spNetworkPacket& packet,std::uint16_t type,std::uint16_t destination,
        const std::vector<std::uint8_t>& data,std::uint32_t field1=0,std::uint32_t field2=0)
    {
        spNetworkPacketHeaderForAnalysis header;header.source=0;header.destination=destination;
        header.packetType=type;header.useTcp=1;header.dataField1=field1;header.dataField2=field2;header.size=static_cast<std::uint16_t>(data.size());
        packet.SetHeaderForAnalysis(header);packet.SetPayloadForAnalysis(data.data(),data.size());
    }
    void Run()
    {
        Graph graph;Check(graph.manager->StartForAnalysis(),"real Manager starts real component graph");
        Check(graph.server&&graph.peer&&graph.controller&&graph.liveNetworks==4,"all real components created");
        Check(graph.controller->GetStateForAnalysis().current==0,"initial controller state");
        // This forwarding-byte producer is deliberately a fixture input.
        // Its existence does not establish original application bootstrap.
        graph.manager->SetControlsForAnalysis(1,0,1);
        graph.controller->RequestForAnalysis(4,1);
        Check(graph.controller->GetStateForAnalysis().current==1,"event4 enters connecting state");
        const std::vector<std::uint8_t> probe{0xd0,0xba,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0x2a};
        Check(graph.wire.size()==1&&graph.wire[0]==probe,"exact Probe bytes from real Peer/Packet/MemoryStream/SocketStream");
        Check(graph.manager->GetStateForAnalysis().statistics[0]==1,"actual Probe success counter");

        spNetworkPacket initialReply;Fill(initialReply,1,0xbad0,{0x68,0x24});
        spMemoryStream initialBytes;Check(initialBytes.Open("initial-reply"),"initial-reply stream");
        Check(initialReply.WriteForAnalysis(initialBytes),"serialize actual InitialInfoReply");
        Check(graph.server->ProcessPacketsForAnalysis(nullptr,initialBytes),"real Server parses and handles InitialInfoReply");
        Check(graph.server->GetStateForAnalysis().id==0x2468,"Server stores initial ID");
        Check(graph.controller->GetStateForAnalysis().current==1&&graph.controller->GetStateForAnalysis().queued==std::vector<std::uint32_t>{5},"InitialInfoReply queues event5 without committing state");
        Check(graph.manager->PumpForAnalysis(),"Manager pumps actual state2 entry");
        Check(graph.controller->GetStateForAnalysis().current==2,"event5 enters state2");
        const std::vector<std::uint8_t> ready{0x68,0x24,0,0,3,0,1,0,0,0,0,0,0,0,0,1,0,1};
        Check(graph.wire.size()==2&&graph.wire[1]==ready,"exact Ready bytes, including original Source/Destination mapping");
        Check(graph.readyObservers==std::vector<Graph::Handle>({0x100,0x101,0x102}),"all observers notified in order on state2 entry");
        const auto observed=std::find(graph.order.begin(),graph.order.end(),"observer-ready");
        const auto sent=std::find(graph.order.begin(),graph.order.end(),"wire:3");
        Check(observed!=graph.order.end()&&sent!=graph.order.end()&&observed<sent,"notifications precede actual Ready send");

        spNetworkPacket gameStart,generic;Fill(gameStart,4,0x2468,{1,0,0x34,0x12});
        Fill(generic,2,0x2468,{0x9a,0xbc},0xaced,0x5432);
        spMemoryStream packets;Check(packets.Open("game-start-and-generic"),"packet stream");
        Check(gameStart.WriteForAnalysis(packets)&&generic.WriteForAnalysis(packets),"serialize two real packets");
        Check(graph.server->ProcessPacketsForAnalysis(graph.peer.get(),packets),"Server parses complete consecutive packets");
        Check(graph.queued.size()==2&&graph.manager->GetStateForAnalysis().queued==2,"queue adapter holds two actual packet owners");
        Check(graph.manager->GetStateForAnalysis().uniqueId==0,"GameStart remains queued until Manager Pump");
        Check(graph.manager->PumpForAnalysis(),"Manager dispatches real queued packets");
        Check(graph.manager->GetStateForAnalysis().uniqueId==0x1234,"GameStart assigns Manager unique ID");
        Check(graph.queued.empty()&&graph.deletedTypes==std::vector<std::uint16_t>({4,2}),"Manager releases each queued actual packet once in order");
        Check(graph.dataObservers==std::vector<Graph::Handle>{0x100},"type2 uses actual class/key filtering");
        Check(graph.controller->GetStateForAnalysis().current==2&&graph.controller->GetStateForAnalysis().queued==std::vector<std::uint32_t>{7},"GameStart queues event7 after packet dispatch");
        Check(graph.manager->PumpForAnalysis(),"Manager pumps event7");
        Check(graph.controller->GetStateForAnalysis().current==3&&graph.timerRefreshes==1,"state3 entry refreshes actual Manager timer boundary");
        Check(graph.wire.size()==2,"literal zero timer does not synthesize a Hello send");
        Check(graph.peer->GetStateForAnalysis().id==0,"Manager unique ID is not silently copied into Peer");
        const auto state=graph.manager->GetStateForAnalysis();
        Check(state.statistics[0]==1&&state.statistics[3]==1&&state.statistics[15]==1&&state.statistics[16]==1&&state.statistics[18]==2,"cross-component counters, including native double GameStart increment");
        Check(graph.dumped==std::vector<std::uint16_t>({1,4,2})&&graph.packetsCreated==5,"actual parser creates EOF probes without queueing them");
        Check(graph.transitions==std::vector<std::pair<std::uint32_t,std::uint8_t>>({{5,0},{7,0}}),"actual Server and Manager transition requests");
        Check(graph.matchPumps==3,"each actual Manager Pump reaches matchmaking boundary once");
        graph.Close();
    }
}
int main()
{
    try{Run();std::cout<<"Network component graph flow PASS (foreign platform fixtures only)\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
