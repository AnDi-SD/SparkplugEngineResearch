#include "Code/Sparkplug/spNetworkPeer.h"
#include "Analysis/PC/spNetworkPeerAbi.h"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
    std::string Hex(const void* data,std::size_t size)
    {
        const char digits[]="0123456789abcdef";std::string result;
        auto* bytes=static_cast<const std::uint8_t*>(data);
        for(std::size_t i=0;i<size;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;
    }
    std::vector<std::uint8_t> Payload(std::size_t size)
    {std::vector<std::uint8_t> data(size);for(std::size_t i=0;i<size;++i)data[i]=static_cast<std::uint8_t>(i*37+11);return data;}
    struct Host;
    class Network final:public spNetwork
    {
    public:
        Host& host;unsigned id;
        Network(Host& h,unsigned value):host(h),id(value){}
        ~Network() override;
        void Connected(std::uint8_t value){connected_=value;}
        bool OpenForAnalysis(std::uint32_t) override;
        bool ConnectForAnalysis(std::uint32_t,std::uint16_t) override;
        bool CloseForAnalysis() override;
        bool SetBlockingForAnalysis(std::uint8_t) override;
        int SendForAnalysis(const void*,int) override;
        bool BindForAnalysis(std::uint32_t,std::uint16_t) override{throw std::logic_error("Unexpected bind");}
        bool ListenForAnalysis() override{throw std::logic_error("Unexpected listen");}
        spNetworkAcceptForAnalysis AcceptForAnalysis() override{throw std::logic_error("Unexpected accept");}
        bool AddToSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected add");}
        bool RemoveFromSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected remove");}
        bool ClearSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected clear");}
        bool SelectForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected select");}
        int SendToForAnalysis(std::uint32_t,std::uint16_t,const void*,int) override{throw std::logic_error("Unexpected sendto");}
        int ReceiveForAnalysis(void*,int) override{throw std::logic_error("Unexpected recv");}
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void*,int) override{throw std::logic_error("Unexpected recvfrom");}
        bool GetLocalAddressForAnalysis(std::uint32_t&) override{throw std::logic_error("Unexpected address");}
    };
    struct Host final:spNetworkPeerHost,spSocketStreamHost,std::enable_shared_from_this<Host>
    {
        std::string mode;std::vector<std::string> events;unsigned created=0,live=0;int sendResult=23;
        std::unique_ptr<spNetwork> CreateNetwork() override
        {++live;events.push_back("create:"+std::to_string(++created));return std::make_unique<Network>(*this,created);}
        std::unique_ptr<spSocketStream> CreateSocketStream() override
        {return std::make_unique<spSocketStream>(shared_from_this());}
        void ReportInvalidNetwork() override{throw std::logic_error("Unexpected invalid network");}
        Handle ResolveNetworkDebug() override{events.push_back("resolve-debug");return mode.find("no-debug")==std::string::npos?1:0;}
        void Log(Handle handle,std::int32_t level,const std::string& category,const std::string& text) override
        {Check(handle==1&&level==0&&category=="Network Peer","exact logger arguments");events.push_back("log:"+text);}
        bool Success(const std::string& name,unsigned id)
        {
            const auto channel=id==2?"udp":"tcp";
            return mode!="fail-"+std::string(channel)+"-"+name&&mode!="fail-open-no-debug"&&
                !(mode=="adopt-fail-"+name&&id==2)&&!(mode=="close-fail"&&name=="close");
        }
    };
    Network::~Network(){host.events.push_back("delete:"+std::to_string(id));--host.live;}
    bool Network::OpenForAnalysis(std::uint32_t type)
    {host.events.push_back("open:"+std::to_string(id)+":"+std::to_string(type));type_=type;return host.Success("open",id);}
    bool Network::ConnectForAnalysis(std::uint32_t address,std::uint16_t port)
    {
        host.events.push_back("connect:"+std::to_string(id)+":"+std::to_string(address)+":"+std::to_string(port));
        if(!host.Success("connect",id))return false;connected_=1;address_=address;port_=port;return true;
    }
    bool Network::CloseForAnalysis()
    {host.events.push_back("close:"+std::to_string(id));connected_=0;return host.Success("close",id);}
    bool Network::SetBlockingForAnalysis(std::uint8_t value)
    {host.events.push_back("blocking:"+std::to_string(id)+":"+std::to_string(value));return host.Success("blocking",id);}
    int Network::SendForAnalysis(const void* data,int size)
    {
        Check(size>=0&&size<=273,"bounded full packet send");
        host.events.push_back("send:"+std::to_string(id)+":"+std::to_string(size)+":"+Hex(data,size)+":"+std::to_string(host.sendResult));return host.sendResult;
    }
    const char* Modes[]={"defaults","clone","copy","copy-self","notify","open-both","open-tcp-only","open-raw255",
        "fail-tcp-open","fail-udp-open","fail-tcp-connect","fail-udp-connect","fail-tcp-blocking","fail-udp-blocking","fail-open-no-debug",
        "close","close-fail","close-twice","adopt","adopt-fail-connect","adopt-fail-blocking",
        "send-tcp","send-udp","send-raw255","send-empty","send-full256","send-zero","send-short","send-error","send-negative",
        "send-repeat","send-disconnected","send-disconnected-no-debug","send-broadcast-source","send-packet-rewrite","send-packet-disconnected-rewrite"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known peer case");
        auto h=std::make_shared<Host>();h->mode=mode;SetNetworkPeerFactoryHostForAnalysis(h);
        auto peer=std::make_unique<spNetworkPeer>();spCloneManager clones;
        Check(peer->IsExactly(spNetworkPeer::ClassID)&&peer->IsKindOf(spBaseObject::ClassID),"peer RTTI");
        std::vector<std::string> states;std::vector<int> results;
        auto State=[&](spNetworkPeer& p)
        {
            const auto s=p.GetStateForAnalysis();auto& tcp=p.TcpForAnalysis();auto& udp=p.UdpForAnalysis();
            std::uint32_t size=0,position=0;(void)p.BufferForAnalysis().GetSize(&size);(void)p.BufferForAnalysis().GetCurrentPosition(position);
            std::ostringstream o;o<<'['<<s.id<<','<<unsigned(s.byte12)<<','<<unsigned(s.unknown13)<<','<<s.timestamp<<','
                <<static_cast<Network&>(tcp.NetworkForAnalysis()).id<<','<<static_cast<Network&>(udp.NetworkForAnalysis()).id<<','
                <<tcp.TypeForAnalysis()<<','<<tcp.AddressForAnalysis()<<','<<tcp.PortWordForAnalysis()<<','
                <<udp.TypeForAnalysis()<<','<<udp.AddressForAnalysis()<<','<<udp.PortWordForAnalysis()<<','<<size<<','<<position<<','<<p.BufferForAnalysis().GetCapacity()<<']';states.push_back(o.str());
        };
        State(*peer);
        peer->SetIdForAnalysis(0x1234);peer->SetTimestampForAnalysis(0x89abcdef);
        peer->TcpForAnalysis().SetEndpointForAnalysis(1,0x01020304,0x1122,0);
        peer->UdpForAnalysis().SetEndpointForAnalysis(0,0x05060708,0x3344,0);
        if(mode=="clone"||mode=="copy")
        {
            auto other=mode=="clone"?peer->vfunc_10(clones):std::make_unique<spNetworkPeer>();
            if(mode=="copy")results.push_back(peer->vfunc_14(*other,clones));
            else results.push_back(clones.FindClone(*peer)==other.get());
            State(*peer);State(static_cast<spNetworkPeer&>(*other));other.reset();
        }
        else if(mode=="copy-self"){results.push_back(peer->vfunc_14(*peer,clones));State(*peer);}
        else if(mode=="notify"){peer->vfunc_0C(nullptr);State(*peer);}
        else if(mode.rfind("open",0)==0||mode.rfind("fail-",0)==0)
        {results.push_back(peer->OpenForAnalysis(mode=="open-tcp-only"?1:mode=="open-raw255"?255:0));State(*peer);}
        else if(mode.rfind("close",0)==0)
        {results.push_back(peer->CloseForAnalysis());if(mode=="close-twice")results.push_back(peer->CloseForAnalysis());State(*peer);}
        else if(mode.rfind("adopt",0)==0)
        {
            auto incoming=h->CreateSocketStream();incoming->SetEndpointForAnalysis(1,0xaabbccdd,9999,255);
            static_cast<Network&>(incoming->NetworkForAnalysis()).Connected(1);
            results.push_back(peer->AdoptTcpForAnalysis(std::move(incoming),0xcdef));State(*peer);
        }
        else if(mode.rfind("send",0)==0)
        {
            static_cast<Network&>(peer->TcpForAnalysis().NetworkForAnalysis()).Connected(mode.find("disconnected")==std::string::npos?1:0);
            h->sendResult=mode=="send-zero"?0:mode=="send-short"?1:mode=="send-error"?-1:mode=="send-negative"?-7:23;
            auto payload=Payload(mode=="send-empty"?0:mode=="send-full256"?256:5);
            const auto tcp=std::uint8_t(mode=="send-udp"?0:mode=="send-raw255"?255:1);
            if(mode.rfind("send-packet",0)==0)
            {
                spNetworkPacket packet;spNetworkPacketHeaderForAnalysis header;
                header.source=0x5678;header.destination=0xabcd;header.packetType=0x42;header.useTcp=tcp;
                header.dataField1=0x11223344;header.dataField2=0x55667788;header.size=static_cast<std::uint16_t>(payload.size());
                packet.SetHeaderForAnalysis(header);packet.SetPayloadForAnalysis(payload.data(),payload.size());
                results.push_back(peer->SendPacketForAnalysis(packet,255));results.push_back(packet.HeaderForAnalysis().destination);
            }
            else
            {
                results.push_back(peer->SendForAnalysis(0xabcd,0x42,payload.data(),static_cast<std::uint16_t>(payload.size()),tcp,mode=="send-broadcast-source"?255:0,0x11223344,0x55667788));
                if(mode=="send-repeat")results.push_back(peer->SendForAnalysis(0xabcd,0x42,payload.data(),static_cast<std::uint16_t>(payload.size()),tcp,0,0x11223344,0x55667788));
            }
            State(*peer);
        }
        else State(*peer);
        peer.reset();Check(!h->live,"all peer network dependencies released");SetNetworkPeerFactoryHostForAnalysis(nullptr);
        std::ostringstream out;out<<"{\"results\":[";
        for(std::size_t i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"],\"states\":[";
        for(std::size_t i=0;i<states.size();++i){if(i)out<<',';out<<states[i];}out<<"],\"events\":[";
        for(std::size_t i=0;i<h->events.size();++i){if(i)out<<',';out<<'"'<<h->events[i]<<'"';}out<<"]}";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        SetNetworkPeerFactoryHostForAnalysis(nullptr);bool rejected=false;
        try{spNetworkPeer peer;}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"missing peer host rejected");
        for(const auto mode:Modes)(void)Run(mode);
        std::cout<<"spNetworkPeer cases PASS: "<<std::size(Modes)<<'\n';return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
