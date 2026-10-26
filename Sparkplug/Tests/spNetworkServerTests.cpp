#include "Code/Sparkplug/spNetworkServer.h"
#include "Analysis/PC/spNetworkServerAbi.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
    std::string Quote(const std::string& input)
    {std::string out="\"";for(char c:input){if(c=='"'||c=='\\')out+='\\';out+=c;}return out+'"';}
    struct Host;
    class Network final:public spNetwork
    {
    public:
        Host& host;unsigned number;std::string label;
        Network(Host& h,unsigned n):host(h),number(n){connected_=1;type_=1;address_=0xffffffff;port_=0xffff;}
        ~Network() override;
        void Connected(std::uint8_t value){connected_=value;}
        bool OpenForAnalysis(std::uint32_t) override;
        bool BindForAnalysis(std::uint32_t,std::uint16_t) override{return true;}
        bool ListenForAnalysis() override{return true;}
        bool ConnectForAnalysis(std::uint32_t,std::uint16_t) override{connected_=1;return true;}
        bool CloseForAnalysis() override;
        bool SetBlockingForAnalysis(std::uint8_t) override{return true;}
        int SendForAnalysis(const void*,int) override;
        spNetworkAcceptForAnalysis AcceptForAnalysis() override;
        bool AddToSetForAnalysis(std::uint32_t) override;
        bool RemoveFromSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected remove-set");}
        bool ClearSetForAnalysis(std::uint32_t) override;
        bool SelectForAnalysis(std::uint32_t) override;
        int SendToForAnalysis(std::uint32_t,std::uint16_t,const void*,int) override{throw std::logic_error("Unexpected sendto");}
        int ReceiveForAnalysis(void*,int) override;
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void*,int) override{throw std::logic_error("Unexpected receivefrom");}
        bool GetLocalAddressForAnalysis(std::uint32_t&) override{throw std::logic_error("Unexpected address");}
    };
    struct Host final:spNetworkServerHost,spNetworkPeerHost,spSocketStreamHost,spNetworkPacketHost,std::enable_shared_from_this<Host>
    {
        std::string mode;std::vector<std::string> events;
        std::vector<std::vector<std::uint8_t>> sent;
        unsigned networks=0,live=0,depth=0,stopByte=0,deletedQueues=0;
        std::uint32_t errors=0;bool debug=false;
        std::uint32_t controller=1,ticks=1000;unsigned packetsCreated=0;
        std::array<std::uint32_t,28> statistics{};
        std::vector<std::unique_ptr<spNetworkPacket>> queued,unqueued;
        std::vector<std::unique_ptr<spNetworkPeer>> unaccepted;
        std::vector<std::uint8_t> receivePacket;
        struct ReceiveChunk{int result;std::vector<std::uint8_t> data;};
        struct ReceiveRecord{unsigned iteration;std::string label;int result;std::vector<std::uint8_t> data;};
        struct BufferRecord{unsigned iteration;std::uint32_t size,position;std::vector<std::uint8_t> data;};
        std::vector<ReceiveChunk> receiveChunks;std::vector<ReceiveRecord> receiveRecords;std::vector<BufferRecord> bufferRecords;
        std::size_t receiveIndex=0;unsigned workerIterations=0,workerLimit=1,peerTcpDeletes=0;
        spNetworkServer* workerServer=nullptr;
        struct LogRecord{std::int32_t level;std::string category,text;};std::vector<LogRecord> logs;
        bool R3() const{return mode.rfind("r3-",0)==0;}
        bool R4() const{return mode.rfind("r4-",0)==0;}
        bool WorkerFixture() const{return R3()||R4();}
        using Handle=std::uintptr_t;
        Handle CreatePeerLock() override{events.push_back("create-lock");return 1;}
        void FreeLockStorage(Handle value) noexcept override{if(value!=1||depth)std::terminate();events.push_back("free-lock");}
        void EnterLock(Handle value) override{Check(value==1,"exact peer lock");++depth;events.push_back("enter:peers");}
        void LeaveLock(Handle value) noexcept override{if(value!=1||!depth)std::terminate();--depth;events.push_back("leave:peers");}
        Handle CreateThread() override{events.push_back("create-thread");return 2;}
        void DeleteThread(Handle value) noexcept override{if(value!=2)std::terminate();events.push_back("delete-thread-secondary+4");}
        bool ThreadCreate(Handle value,std::uint32_t token,spNetworkServer& body) override
        {Check(value==2&&token==0x484800&&body.IsExactly(spNetworkServer::ClassID),"thread callback token and body");events.push_back("thread-create:484800");return mode!="open-thread-fail";}
        bool ThreadResume(Handle) override{events.push_back("thread-resume");return mode!="open-resume-fail";}
        bool ThreadIsRunning(Handle) override{events.push_back("thread-running");return mode.rfind("destroy-",0)==0;}
        void SetThreadStopByte(Handle,std::uint8_t value) override{stopByte=value;}
        std::uint8_t ReadThreadStopByte(Handle value) override{Check(value==2,"worker thread");return static_cast<std::uint8_t>(stopByte);}
        bool ThreadWait(Handle,std::uint32_t ms) override{Check(ms==1000&&stopByte==1,"stop before bounded wait");events.push_back("thread-wait:1000");return mode!="destroy-wait-timeout";}
        std::unique_ptr<spNetwork> CreateNetwork() override
        {const auto n=networks++;++live;if(n<2)events.push_back(n?"create:udp":"create:tcp");auto result=std::make_unique<Network>(*this,n);result->label=n==0?"tcp":n==1?"udp":"unassigned";return result;}
        std::unique_ptr<spSocketStream> CreateSocketStream() override{return std::make_unique<spSocketStream>(shared_from_this());}
        void ReportInvalidNetwork() override{throw std::logic_error("Unexpected invalid network");}
        Handle ResolveNetworkDebug() override{return debug?10:0;}
        std::int32_t AdmissionLogLevel() override{Check(mode=="r3-add-adopt-fail-debug","explicit qualified admission level input");return 1;}
        void Log(Handle,std::int32_t level,const std::string& category,const std::string& text,const std::vector<std::uint32_t>&) override
        {Check(category=="Network Server","server category");events.push_back("log:"+text);logs.push_back({level,category,text});}
        void Log(Handle,std::int32_t,const std::string&,const std::string&) override{}
        void ManagerError(std::uint32_t value) override{errors=value;events.push_back("error:"+std::to_string(value));}
        void ManagerTransition(std::uint32_t a,std::uint8_t b) override{events.push_back("transition:"+std::to_string(a)+":"+std::to_string(b));}
        Timer ReadManagerTimer() override{return {0,ticks,1};}
        void StartManagerTimer() override{throw std::logic_error("Unexpected timer start");}
        void RefreshManagerTimer() override{throw std::logic_error("Unexpected timer refresh");}
        std::uint32_t ReadManagerControllerState() override{events.push_back("enter:controller");events.push_back("leave:controller");return controller;}
        void IncrementManagerStatistic(std::size_t index) override{Check(index<statistics.size(),"bounded statistic");++statistics[index];}
        std::unique_ptr<spNetworkPacket> CreatePacket() override{++packetsCreated;return std::make_unique<spNetworkPacket>(shared_from_this());}
        void DumpPacket(Handle,const spNetworkPacket&) override{events.push_back("packet-dump");}
        void EnqueuePacket(Handle value,std::unique_ptr<spNetworkPacket> packet) override
        {Check(value==11&&packet!=nullptr,"queue consumes actual packet");events.push_back("enter:queue");queued.push_back(std::move(packet));events.push_back("leave:queue");}
        void RetainUnqueuedPacket(std::unique_ptr<spNetworkPacket> packet) override{Check(packet!=nullptr,"retained allocation");unqueued.push_back(std::move(packet));}
        std::unique_ptr<spNetworkPeer> CreatePeer() override
        {auto result=std::make_unique<spNetworkPeer>(shared_from_this());static_cast<Network&>(result->UdpForAnalysis().NetworkForAnalysis()).label="admission-udp";return result;}
        void RetainUnacceptedPeer(std::unique_ptr<spNetworkPeer> peer) override{Check(peer!=nullptr,"orphan owner");unaccepted.push_back(std::move(peer));}
        void ReportFailure(const char* message) override{events.push_back(std::string("packet-error:")+message);}
        void DeleteQueue(Handle value) override{Check(value==11,"old queue identity");++deletedQueues;}
    };
    Network::~Network(){if(number<2)host.events.push_back(number?"delete:udp":"delete:tcp");if(label=="peer0")++host.peerTcpDeletes;--host.live;}
    bool Network::OpenForAnalysis(std::uint32_t value)
    {
        type_=value;if(number<2)host.events.push_back(number?"open:udp":"open:tcp");
        return host.mode!=(number?"open-udp-fail":"open-tcp-fail")&&!(host.mode.rfind("r3-add-adopt-fail",0)==0&&label=="admission-udp");
    }
    bool Network::CloseForAnalysis()
    {
        connected_=0;if(number<2)host.events.push_back(number?"close:udp":"close:tcp");
        else if(host.WorkerFixture()?label.rfind("peer",0)==0:number%2==0)host.events.push_back("peer-close");
        return host.mode!=(number?"close-udp-fail":"close-tcp-fail");
    }
    int Network::SendForAnalysis(const void* input,int count)
    {
        Check(count>=0&&count<=273,"bounded packet write");
        const auto* bytes=static_cast<const std::uint8_t*>(input);host.sent.emplace_back(bytes,bytes+count);
        return host.mode=="hello-send-zero"||host.mode=="r3-request-send-zero"?0:host.mode=="hello-send-negative"||host.mode=="r3-request-send-negative"?-1:count;
    }
    spNetworkAcceptForAnalysis Network::AcceptForAnalysis()
    {Check(host.WorkerFixture(),"worker accept fixture");host.events.push_back("accept");return {};}
    bool Network::AddToSetForAnalysis(std::uint32_t index)
    {Check(host.WorkerFixture()&&index<3,"worker add fixture");host.events.push_back("add-set:"+label+":"+std::to_string(index));return true;}
    bool Network::ClearSetForAnalysis(std::uint32_t index)
    {Check(host.WorkerFixture()&&index<3,"worker clear fixture");host.events.push_back("clear-set:"+label+":"+std::to_string(index));return (host.mode=="r3-worker-clear0"&&index==0)||(host.mode=="r3-worker-clear1"&&index==1);}
    bool Network::SelectForAnalysis(std::uint32_t milliseconds)
    {
        Check(host.WorkerFixture()&&milliseconds==1000,"bounded worker iteration");host.events.push_back("select:"+label+":1000");
        if(host.R4())
        {
            Check(host.workerServer!=nullptr,"R4 observes actual worker buffer at foreign select boundary");
            auto& buffer=host.workerServer->BufferForAnalysis();std::uint32_t size=0,position=0;
            Check(buffer.GetSize(&size)&&buffer.GetCurrentPosition(position),"actual worker memory state");
            const auto* data=static_cast<const std::uint8_t*>(buffer.GetBuffer());
            host.bufferRecords.push_back({host.workerIterations,size,position,{data,data+size}});
        }
        if(++host.workerIterations>=host.workerLimit)host.stopByte=1;return false;
    }
    int Network::ReceiveForAnalysis(void* output,int capacity)
    {
        Check(host.WorkerFixture()&&capacity==500,"real SocketStream 500-byte receive");
        if(host.R4())
        {
            Host::ReceiveChunk chunk{-1,{}};
            if(label!="udp")
            {
                Check(label=="peer0"&&host.receiveIndex<host.receiveChunks.size(),"bounded R4 receive sequence");
                chunk=host.receiveChunks[host.receiveIndex++];
            }
            Check(chunk.result>=-1&&chunk.result<=500&&(chunk.result==-1?chunk.data.empty():chunk.data.size()==static_cast<std::size_t>(chunk.result)),"qualified foreign receive result");
            if(chunk.result>0)std::memcpy(output,chunk.data.data(),chunk.data.size());
            host.receiveRecords.push_back({host.workerIterations,label,chunk.result,chunk.data});
            host.events.push_back("network-receive:"+label+":"+std::to_string(chunk.result));return chunk.result;
        }
        const bool packet=(label=="udp"&&host.mode=="r3-worker-udp-packet")||(label=="peer0"&&host.mode=="r3-worker-peer-packet");
        if(packet){Check(host.receivePacket.size()<500,"single bounded receive");std::memcpy(output,host.receivePacket.data(),host.receivePacket.size());host.events.push_back("receive:"+label+":"+std::to_string(host.receivePacket.size()));return static_cast<int>(host.receivePacket.size());}
        if(label=="peer0"&&host.mode!="r3-worker-peer-fail"){host.events.push_back("receive:"+label+":0");return 0;}
        host.events.push_back("receive:"+label+":fail");return -1;
    }
    const char* Modes[]={"defaults","open","open-server","open-thread-fail","open-resume-fail","open-udp-fail",
        "close-udp-fail","destroy-wait-timeout","set-queue","replace-queue","release-id","release-duplicates",
        "initial-reply","initial-reply-invalid","set-peer","prune-borrowed","prune-adjacent","hello","hello-send-zero","hello-send-negative","hello-tail","ready","ready-valid","ready-zero"};
    template<class Values> void Array(std::ostream& out,const Values& values)
    {out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<value;}out<<']';}
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known server case");
        auto h=std::make_shared<Host>();h->mode=mode;SetNetworkServerFactoryHostForAnalysis(h);SetNetworkPeerFactoryHostForAnalysis(h);
        auto server=std::make_unique<spNetworkServer>();
        Check(server->IsExactly(spNetworkServer::ClassID)&&server->IsKindOf(spBaseObject::ClassID),"server RTTI");
        Check(!server->GetStateForAnalysis().maximumId,"native unwritten maximum ID");
        Check(std::string(server->BufferForAnalysis().GetStreamName())=="Packet buffer","exact buffer name");
        std::vector<unsigned> results;std::vector<std::unique_ptr<spNetworkPeer>> borrowed;std::optional<std::uint8_t> readyByte;
        auto peer=[&](std::uint16_t id,bool owned,bool connected)
        {
            auto p=std::make_unique<spNetworkPeer>();p->SetIdForAnalysis(id);
            if(owned)Check(p->AdoptTcpForAnalysis(h->CreateSocketStream(),1000),"accepted peer dependency");
            static_cast<Network&>(p->TcpForAnalysis().NetworkForAnalysis()).Connected(connected?1:0);return p;
        };
        if(mode=="open"||mode.rfind("open-",0)==0)
        {server->SetIdForAnalysis(mode=="open-server"?0:1);results.push_back(server->OpenForAnalysis());}
        else if(mode=="close-udp-fail")
        {server->SetStateForAnalysis(0,10,0);results.push_back(server->CloseForAnalysis());}
        else if(mode=="set-queue")
        {results.push_back(server->SetQueueForAnalysis(11));}
        else if(mode=="replace-queue")
        {Check(server->SetQueueForAnalysis(11),"first queue");results.push_back(server->SetQueueForAnalysis(12));Check(h->deletedQueues==1,"old queue release requested once");}
        else if(mode=="release-id"||mode=="release-duplicates")
        {
            server->SetStateForAnalysis(0,10,0);for(auto id:(mode=="release-id"?std::vector<int>{4,2}:std::vector<int>{4,2,5,1,4,2}))server->ReleaseIdForAnalysis(static_cast<std::uint16_t>(id));
        }
        else if(mode.rfind("initial-reply",0)==0)
        {
            server->SetStateForAnalysis(1,10,0);spNetworkPacket packet;
            auto header=packet.HeaderForAnalysis();header.source=2;header.destination=1;header.packetType=1;header.useTcp=1;header.dataField1=3;header.dataField2=4;header.size=2;packet.SetHeaderForAnalysis(header);
            const std::uint8_t valid[]={7,0},invalid[]={0xd0,0xba};packet.SetPayloadForAnalysis(mode=="initial-reply"?valid:invalid,2);
            results.push_back(server->HandleInitialReplyForAnalysis(packet));
        }
        else if(mode=="set-peer")
        {borrowed.push_back(peer(7,false,true));results.push_back(server->SetPeerForAnalysis(*borrowed.back()));}
        else if(mode=="prune-borrowed")
        {
            borrowed.push_back(peer(7,false,false));Check(server->SetPeerForAnalysis(*borrowed.back()),"borrowed peer");h->events.resize(4);
            server->SetStateForAnalysis(1,10,1);server->PruneDisconnectedForAnalysis();Check(h->errors==4,"borrowed peer error");
        }
        else if(mode=="prune-adjacent")
        {
            server->AdoptPeerForAnalysis(peer(7,true,false));server->AdoptPeerForAnalysis(peer(8,true,false));h->events.resize(4);
            server->SetStateForAnalysis(1,10,2);server->PruneDisconnectedForAnalysis();
            Check(server->GetStateForAnalysis().peerIds==std::vector<std::uint16_t>{8},"native skips adjacent disconnected peer");Check(server->GetStateForAnalysis().acceptedCount==1,"guarded accepted decrement");
        }
        else if(mode.rfind("ready",0)==0)
        {
            server->SetStateForAnalysis(0,10,0);auto value=peer(mode=="ready"?7:1,mode!="ready",true);auto* pointer=value.get();
            if(mode=="ready"){Check(server->SetPeerForAnalysis(*pointer),"borrowed ready peer");borrowed.push_back(std::move(value));}
            else server->AdoptPeerForAnalysis(std::move(value));
            h->events.resize(4);spNetworkPacket packet;auto header=packet.HeaderForAnalysis();header.source=mode=="ready"?7:1;header.destination=0;header.packetType=3;header.useTcp=1;header.dataField1=3;header.dataField2=4;header.size=4;packet.SetHeaderForAnalysis(header);
            const std::uint8_t payload[]={std::uint8_t(mode=="ready-zero"?0:1),0,0,0};packet.SetPayloadForAnalysis(payload,sizeof(payload));
            results.push_back(server->HandleReadyForAnalysis(pointer,packet));readyByte=pointer->GetStateForAnalysis().unknown13;
            Check(results.back()==unsigned(mode!="ready")&&unsigned(*readyByte)==unsigned(mode=="ready-valid"),"ready payload, owned count and peer byte");
        }
        else if(mode.rfind("hello",0)==0)
        {
            server->SetStateForAnalysis(0,10,0);borrowed.push_back(peer(7,false,true));Check(server->SetPeerForAnalysis(*borrowed.back()),"hello peer");h->events.resize(4);
            spNetworkPacket packet;auto header=packet.HeaderForAnalysis();header.source=7;header.destination=0;header.packetType=5;header.useTcp=1;header.dataField1=3;header.dataField2=4;header.size=mode=="hello-tail"?12:8;packet.SetHeaderForAnalysis(header);
            const std::uint8_t data[]={32,3,0,0,0,0,0,0,0xaa,0xbb,0xcc,0xdd};packet.SetPayloadForAnalysis(data,header.size);
            const bool accepted=server->HandleHelloForAnalysis(borrowed.back().get(),packet);
            Check(accepted==(mode=="hello"||mode=="hello-tail"),"positive send required for hello response");
            Check(borrowed.back()->GetStateForAnalysis().timestamp==1000,"hello receive timestamp");
            const auto* updated=packet.KnownPayloadForAnalysis(header.size);Check(updated[4]==232&&updated[5]==3,"mutate timer word");
            if(mode=="hello-tail")Check(updated[8]==0xaa&&updated[11]==0xdd,"retain payload tail");
            Check(h->sent.size()==1&&h->sent[0].size()==25&&h->sent[0][0]==0&&h->sent[0][2]==7&&h->sent[0][4]==6,"shared peer packet source/destination/type");
            results.push_back(accepted);if(!accepted)Check(h->errors==8,"zero/negative hello send error");
        }
        const auto state=server->GetStateForAnalysis();
        if(mode=="prune-borrowed")Check(state.peerIds==std::vector<std::uint16_t>{7},"borrowed disconnected peer retained");
        server.reset();Check(!h->depth,"all peer lock scopes leave");
        if(mode=="destroy-wait-timeout")Check(h->stopByte==1,"stop byte written even timeout");
        std::ostringstream out;out<<"{\"events\":[";for(std::size_t i=0;i<h->events.size();++i){if(i)out<<',';out<<Quote(h->events[i]);}
        out<<"],\"results\":";Array(out,results);out<<",\"state\":{\"id\":"<<state.id<<",\"queue\":"<<(state.hasQueue?"true":"false")<<",\"count\":"<<state.acceptedCount<<",\"peerIds\":";
        Array(out,state.peerIds);out<<",\"freeValues\":";Array(out,state.availableIds);out<<"}";
        if(readyByte)out<<",\"readyByte\":"<<unsigned(*readyByte);out<<"}";
        borrowed.clear();Check(!h->live,"all stream/network dependencies released");SetNetworkServerFactoryHostForAnalysis(nullptr);SetNetworkPeerFactoryHostForAnalysis(nullptr);return out.str();
    }
    const char* R3Modes[]={"r3-id-empty","r3-id-pool","r3-request-source-zero","r3-request-pool","r3-request-repeat",
        "r3-request-wrong-state","r3-request-state2","r3-request-send-zero","r3-request-send-negative",
        "r3-dispatch-empty","r3-dispatch-queue","r3-dispatch-unqueued","r3-dispatch-wrong-destination",
        "r3-dispatch-broadcast-client","r3-dispatch-broadcast-server","r3-dispatch-type0","r3-dispatch-type1",
        "r3-dispatch-type3","r3-dispatch-type5","r3-dispatch-mixed","r3-dispatch-truncated",
        "r3-worker-stopped","r3-worker-server-empty","r3-worker-clear0","r3-worker-clear1","r3-worker-peer-eof",
        "r3-worker-peer-packet","r3-worker-peer-fail","r3-worker-timeout","r3-worker-at-timeout","r3-worker-udp-packet",
        "r3-add-full","r3-add-accept","r3-add-adopt-fail","r3-add-adopt-fail-debug"};
    std::string Hex(const std::vector<std::uint8_t>& input)
    {const char digits[]="0123456789abcdef";std::string result;for(auto b:input){result+=digits[b>>4];result+=digits[b&15];}return result;}
    std::vector<std::uint8_t> WireOf(const spNetworkPacket& packet)
    {
        spMemoryStream stream;Check(stream.Open("wire"),"wire output stream");Check(packet.WriteForAnalysis(stream),"serialize actual packet");
        std::uint32_t size=0;Check(stream.GetSize(&size),"wire size");const auto* data=static_cast<const std::uint8_t*>(stream.GetBuffer());return {data,data+size};
    }
    void Fill(spNetworkPacket& packet,std::uint16_t type,std::uint16_t source,std::uint16_t destination,const std::vector<std::uint8_t>& payload)
    {
        auto header=packet.HeaderForAnalysis();header.source=source;header.destination=destination;header.packetType=type;header.useTcp=1;
        header.dataField1=3;header.dataField2=4;header.size=static_cast<std::uint16_t>(payload.size());packet.SetHeaderForAnalysis(header);packet.SetPayloadForAnalysis(payload.data(),payload.size());
    }
    std::vector<std::uint8_t> Wire(std::uint16_t type=9,std::uint16_t source=7,std::uint16_t destination=1,std::vector<std::uint8_t> payload={5,6})
    {spNetworkPacket packet;Fill(packet,type,source,destination,payload);return WireOf(packet);}
    std::string RunR3(const std::string& mode)
    {
        Check(std::find(std::begin(R3Modes),std::end(R3Modes),mode)!=std::end(R3Modes),"known R3 Server case");
        auto h=std::make_shared<Host>();h->mode=mode;SetNetworkServerFactoryHostForAnalysis(h);SetNetworkPeerFactoryHostForAnalysis(h);SetNetworkPacketFactoryHostForAnalysis(h);
        auto server=std::make_unique<spNetworkServer>();server->SetStateForAnalysis(0,10,0);
        std::vector<unsigned> results;std::vector<std::unique_ptr<spNetworkPeer>> borrowed;std::vector<spNetworkPeer*> allPeers;
        auto peer=[&](std::uint16_t id,bool owned=false,bool inList=false)
        {
            auto value=std::make_unique<spNetworkPeer>(h);value->SetIdForAnalysis(id);
            if(owned)Check(value->AdoptTcpForAnalysis(h->CreateSocketStream(),1000),"owned peer dependency");
            static_cast<Network&>(value->TcpForAnalysis().NetworkForAnalysis()).label="peer"+std::to_string(allPeers.size());
            auto* pointer=value.get();allPeers.push_back(pointer);
            if(owned){Check(inList,"owned fixture list transfer");server->AdoptPeerForAnalysis(std::move(value));}
            else {if(inList)Check(server->SetPeerForAnalysis(*pointer),"borrowed fixture list link");borrowed.push_back(std::move(value));}
            return pointer;
        };
        auto pool=[&](){server->ReleaseIdForAnalysis(4);server->ReleaseIdForAnalysis(2);};
        if(mode=="r3-id-empty"||mode=="r3-id-pool")
        {if(mode=="r3-id-pool")pool();h->events.clear();for(unsigned i=0;i<(mode=="r3-id-pool"?3u:1u);++i)results.push_back(server->AllocateIdForAnalysis());}
        else if(mode.rfind("r3-request",0)==0)
        {
            auto* input=peer(mode=="r3-request-repeat"?7:0xbad0);if(mode=="r3-request-pool")pool();
            h->controller=mode=="r3-request-wrong-state"?3:mode=="r3-request-state2"?2:1;
            spNetworkPacket packet;Fill(packet,0,mode=="r3-request-source-zero"?0:2,0,{0x2a});
            h->events.clear();results.push_back(server->HandleInitialRequestForAnalysis(*input,packet));
        }
        else if(mode.rfind("r3-dispatch",0)==0)
        {
            server->SetStateForAnalysis(1,10,0);auto data=Wire();spNetworkPeer* supplied=nullptr;
            if(mode=="r3-dispatch-queue"||mode=="r3-dispatch-mixed"||mode=="r3-dispatch-type1")Check(server->SetQueueForAnalysis(11),"attach queue");
            if(mode=="r3-dispatch-empty")data.clear();
            else if(mode=="r3-dispatch-wrong-destination")data=Wire(9,7,2);
            else if(mode=="r3-dispatch-broadcast-client")data=Wire(9,7,0xffff);
            else if(mode=="r3-dispatch-broadcast-server")
            {server->SetStateForAnalysis(0,10,0);data=Wire(9,7,0xffff);(void)peer(1,true,true);(void)peer(7,true,true);}
            else if(mode=="r3-dispatch-type0")
            {server->SetStateForAnalysis(0,10,0);supplied=peer(0xbad0);pool();data=Wire(0,2,0,{0x2a});}
            else if(mode=="r3-dispatch-type1")
            {data=Wire(1,0,1,{0xef,0xbe});auto next=Wire(9,7,0xbeef);data.insert(data.end(),next.begin(),next.end());}
            else if(mode=="r3-dispatch-type3")
            {server->SetStateForAnalysis(0,10,0);(void)peer(1,true,true);data=Wire(3,1,0,{1});}
            else if(mode=="r3-dispatch-type5")
            {server->SetStateForAnalysis(0,10,0);supplied=peer(7,false,true);data=Wire(5,7,0,{0x20,3,0,0,0,0,0,0});}
            else if(mode=="r3-dispatch-mixed")
            {data=Wire(9,7,2);for(auto next:{Wire(4),Wire(2),Wire(65535,7,0xffff)})data.insert(data.end(),next.begin(),next.end());}
            else if(mode=="r3-dispatch-truncated")data.resize(10);
            spMemoryStream input;Check(input.Open("dispatcher input"),"input stream");if(!data.empty())Check(input.WriteData(data.data(),static_cast<std::uint32_t>(data.size())),"input payload");
            h->events.clear();results.push_back(server->ProcessPacketsForAnalysis(supplied,input));
        }
        else if(mode.rfind("r3-worker",0)==0)
        {
            server->SetStateForAnalysis(mode=="r3-worker-server-empty"?0:1,10,0);h->receivePacket=Wire();
            if(mode=="r3-worker-stopped")h->stopByte=255;
            if(mode.rfind("r3-worker-peer-",0)==0||mode=="r3-worker-timeout"||mode=="r3-worker-at-timeout")
            {(void)peer(7,false,true);if(mode=="r3-worker-timeout")h->ticks=40001;else if(mode=="r3-worker-at-timeout")h->ticks=40000;}
            if(mode=="r3-worker-udp-packet")Check(server->SetQueueForAnalysis(11),"worker queue");
            h->events.clear();server->RunWorkerForAnalysis();
        }
        else
        {
            server->SetStateForAnalysis(0,3,mode=="r3-add-full"?3:0);auto incoming=h->CreateSocketStream();h->events.clear();
            h->debug=mode=="r3-add-adopt-fail-debug";
            results.push_back(server->HandleNewTcpConnectionForAnalysis(incoming));
            Check(bool(incoming)==(mode=="r3-add-full"),"admission transfers stream only after capacity check");
            Check(h->unaccepted.size()==(mode.rfind("r3-add-adopt-fail",0)==0?1u:0u),"failed adoption remains explicitly owned");
            if(!h->unaccepted.empty())Check(h->unaccepted[0]->TcpForAnalysis().NetworkForAnalysis().IsConnectedForAnalysis()!=0,"orphan keeps adopted TCP stream");
        }
        const auto state=server->GetStateForAnalysis();const auto current=server->GetPeerStatesForAnalysis();
        std::ostringstream out;out<<"{\"events\":[";for(std::size_t i=0;i<h->events.size();++i){if(i)out<<',';out<<Quote(h->events[i]);}
        out<<"],\"results\":";Array(out,results);out<<",\"state\":{\"id\":"<<state.id<<",\"max\":"<<*state.maximumId<<",\"queue\":"<<(state.hasQueue?"true":"false")<<",\"count\":"<<state.acceptedCount<<",\"peerIds\":";
        Array(out,state.peerIds);out<<",\"freeValues\":";Array(out,state.availableIds);out<<",\"stats\":";Array(out,h->statistics);
        out<<",\"peers\":[";for(std::size_t i=0;i<allPeers.size();++i){if(i)out<<',';const auto value=allPeers[i]->GetStateForAnalysis();out<<'['<<value.id<<','<<unsigned(value.unknown13)<<','<<value.timestamp<<']';}out<<"]},\"currentPeers\":[";
        for(std::size_t i=0;i<current.size();++i){if(i)out<<',';const auto& p=current[i];out<<'['<<p.id<<','<<unsigned(p.ready)<<','<<p.timestamp<<','<<unsigned(p.connected)<<']';}out<<']';
        auto wires=[&](const char* label,const auto& packets){out<<",\""<<label<<"\":[";bool first=true;for(const auto& p:packets){if(!first)out<<',';first=false;out<<Quote(Hex(WireOf(*p)));}out<<']';};
        out<<",\"sends\":[";for(std::size_t i=0;i<h->sent.size();++i){if(i)out<<',';out<<Quote(Hex(h->sent[i]));}out<<']';wires("queued",h->queued);wires("unqueued",h->unqueued);
        out<<",\"createdPackets\":"<<h->packetsCreated<<",\"retainedPeers\":"<<h->unaccepted.size()<<",\"logs\":[";
        for(std::size_t i=0;i<h->logs.size();++i){if(i)out<<',';const auto& log=h->logs[i];out<<"{\"level\":"<<log.level<<",\"category\":"<<Quote(log.category)<<",\"format\":"<<Quote(log.text)<<'}';}out<<"]}";
        std::vector<std::string> saved;for(auto* packets:{&h->queued,&h->unqueued})for(const auto& p:*packets)saved.push_back(Hex(WireOf(*p)));
        server.reset();std::vector<std::string> after;for(auto* packets:{&h->queued,&h->unqueued})for(const auto& p:*packets)after.push_back(Hex(WireOf(*p)));
        Check(saved==after,"queued and abandoned packets survive Server destruction unchanged");Check(!h->depth,"worker exits every peer lock");
        h->queued.clear();h->unqueued.clear();h->unaccepted.clear();borrowed.clear();Check(!h->live,"all explicit owners release concrete dependencies");
        SetNetworkServerFactoryHostForAnalysis(nullptr);SetNetworkPeerFactoryHostForAnalysis(nullptr);SetNetworkPacketFactoryHostForAnalysis(nullptr);return out.str();
    }
    const char* R4Modes[]={"r4-header-split","r4-payload-split","r4-full-short","r4-full-zero","r4-late-error"};
    std::string RunR4(const std::string& mode)
    {
        Check(std::find(std::begin(R4Modes),std::end(R4Modes),mode)!=std::end(R4Modes),"known R4 framing case");
        auto h=std::make_shared<Host>();h->mode=mode;
        auto server=std::make_unique<spNetworkServer>(h);server->SetStateForAnalysis(1,10,0);Check(server->SetQueueForAnalysis(11),"R4 queue");
        auto peer=std::make_unique<spNetworkPeer>(h);peer->SetIdForAnalysis(7);
        static_cast<Network&>(peer->TcpForAnalysis().NetworkForAnalysis()).label="peer0";
        Check(server->SetPeerForAnalysis(*peer),"R4 borrowed peer lifetime");h->workerServer=server.get();
        auto chunk=[&](const std::vector<std::uint8_t>& data){h->receiveChunks.push_back({static_cast<int>(data.size()),data});};
        std::vector<std::vector<std::uint8_t>> expectedBuffers;std::vector<std::uint32_t> expectedPositions;
        const auto packet19=Wire();
        if(mode=="r4-header-split"||mode=="r4-payload-split")
        {
            const auto packet=mode=="r4-header-split"?packet19:Wire(9,7,1,{5,6,7,8});
            const auto cut=mode=="r4-header-split"?10u:19u;
            const std::vector<std::uint8_t> first(packet.begin(),packet.begin()+cut),second(packet.begin()+cut,packet.end());
            chunk(first);chunk(second);h->workerLimit=2;expectedBuffers={first,second};
            expectedPositions=mode=="r4-header-split"?std::vector<std::uint32_t>{7,7}:std::vector<std::uint32_t>{17,2};
        }
        else
        {
            std::vector<std::uint8_t> firstPayload(250),secondPayload(mode=="r4-full-zero"?216:230);
            for(std::size_t i=0;i<firstPayload.size();++i)firstPayload[i]=static_cast<std::uint8_t>(i);
            for(std::size_t i=0;i<secondPayload.size();++i)secondPayload[i]=static_cast<std::uint8_t>(i);
            auto data=Wire(9,7,1,firstPayload);const auto second=Wire(9,7,1,secondPayload);data.insert(data.end(),second.begin(),second.end());
            Check(data.size()==(mode=="r4-full-zero"?500u:514u),"packet sequence crosses exact receive boundary");
            const std::vector<std::uint8_t> first(data.begin(),data.begin()+500);chunk(first);
            if(mode=="r4-late-error")
            {h->receiveChunks.push_back({-1,{}});chunk(packet19);h->workerLimit=2;expectedBuffers={first,packet19};expectedPositions={500,19};}
            else
            {chunk({data.begin()+500,data.end()});expectedBuffers={data};expectedPositions={static_cast<std::uint32_t>(data.size())};}
        }
        h->events.clear();server->RunWorkerForAnalysis();h->workerServer=nullptr;
        Check(h->workerIterations==h->workerLimit&&h->receiveIndex==h->receiveChunks.size(),"all declared iterations and receive chunks consumed");
        Check(h->bufferRecords.size()==expectedBuffers.size(),"one actual buffer observation per select boundary");
        for(std::size_t i=0;i<expectedBuffers.size();++i)
            Check(h->bufferRecords[i].iteration==i&&h->bufferRecords[i].data==expectedBuffers[i]&&h->bufferRecords[i].position==expectedPositions[i],"partial bytes and parser position are preserved until next Reset");
        const auto queued=mode=="r4-full-short"||mode=="r4-full-zero"?2u:mode=="r4-late-error"?1u:0u;
        Check(h->queued.size()==queued&&h->unqueued.empty(),"fragments and receive-error prefix are not queued");
        for(std::size_t i=0;i<h->queued.size();++i)for(std::size_t j=0;j<i;++j)Check(h->queued[i].get()!=h->queued[j].get(),"distinct actual packet owners");
        const auto state=server->GetStateForAnalysis();const auto current=server->GetPeerStatesForAnalysis();
        std::ostringstream out;out<<"{\"events\":[";for(std::size_t i=0;i<h->events.size();++i){if(i)out<<',';out<<Quote(h->events[i]);}
        out<<"],\"results\":[],\"state\":{\"id\":"<<state.id<<",\"max\":"<<*state.maximumId<<",\"queue\":"<<(state.hasQueue?"true":"false")<<",\"count\":"<<state.acceptedCount<<",\"peerIds\":";
        Array(out,state.peerIds);out<<",\"freeValues\":";Array(out,state.availableIds);out<<",\"freeIds\":"<<state.availableIds.size()<<",\"stats\":";Array(out,h->statistics);
        const auto peerState=peer->GetStateForAnalysis();out<<",\"peers\":[["<<peerState.id<<','<<unsigned(peerState.unknown13)<<','<<peerState.timestamp<<"]]},\"currentPeers\":[";
        for(std::size_t i=0;i<current.size();++i){if(i)out<<',';const auto& p=current[i];out<<'['<<p.id<<','<<unsigned(p.ready)<<','<<p.timestamp<<','<<unsigned(p.connected)<<']';}out<<']';
        std::vector<std::string> saved;out<<",\"queued\":[";for(std::size_t i=0;i<h->queued.size();++i){if(i)out<<',';saved.push_back(Hex(WireOf(*h->queued[i])));out<<Quote(saved.back());}
        out<<"],\"unqueued\":[],\"sends\":[],\"createdPackets\":"<<h->packetsCreated<<",\"workerIterations\":"<<h->workerIterations<<",\"receiveCalls\":[";
        for(std::size_t i=0;i<h->receiveRecords.size();++i)
        {if(i)out<<',';const auto& value=h->receiveRecords[i];out<<"{\"iteration\":"<<value.iteration<<",\"label\":"<<Quote(value.label)<<",\"result\":"<<value.result<<",\"hex\":"<<(value.result==-1?"null":Quote(Hex(value.data)))<<'}';}
        out<<"]}";
        Check(h->peerTcpDeletes==0,"borrowed TCP owner remains alive before Server destruction");
        server.reset();Check(h->peerTcpDeletes==0&&h->live==2,"Server leaves borrowed Peer and its actual TCP/UDP streams alive");
        std::vector<std::string> after;for(const auto& packet:h->queued)after.push_back(Hex(WireOf(*packet)));
        Check(saved==after,"actual queued packet owners survive Server destruction unchanged");
        h->queued.clear();peer.reset();Check(h->peerTcpDeletes==1&&!h->live&&!h->depth,"foreign Peer owner releases TCP network exactly once and every dependency");
        return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--r3-case"){std::cout<<RunR3(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--r4-case"){std::cout<<RunR4(argv[2])<<'\n';return 0;}
        for(const auto* mode:Modes)(void)Run(mode);
        for(const auto* mode:R3Modes)(void)RunR3(mode);
        for(const auto* mode:R4Modes)(void)RunR4(mode);
        std::cout<<"PASS NetworkServer bounded operations and ownership\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
