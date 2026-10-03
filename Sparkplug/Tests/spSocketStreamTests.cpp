#include "Code/SparkBase/spSocketStream.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spSocketStreamAbi.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
    std::string Quote(const std::string& value){return '"'+value+'"';}
    std::string Hex(const void* data,std::size_t size)
    {
        const char digits[]="0123456789abcdef";std::string result;
        auto* p=static_cast<const std::uint8_t*>(data);
        for(std::size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}return result;
    }
    std::vector<std::uint8_t> Payload(unsigned size)
    {std::vector<std::uint8_t> p(size);for(unsigned i=0;i<size;++i)p[i]=static_cast<std::uint8_t>(i*37+11);return p;}
    struct Host;
    class Network final:public spNetwork
    {
    public:
        Host& h;unsigned id;
        Network(Host& owner,unsigned identity):h(owner),id(identity){}
        ~Network() override;
        void Fields(){type_=0x12345678;address_=0xabcdef01;port_=0x5678;}
        bool OpenForAnalysis(std::uint32_t) override;
        bool ConnectForAnalysis(std::uint32_t,std::uint16_t) override;
        bool BindForAnalysis(std::uint32_t,std::uint16_t) override;
        bool ListenForAnalysis() override;
        spNetworkAcceptForAnalysis AcceptForAnalysis() override;
        bool CloseForAnalysis() override;
        bool SetBlockingForAnalysis(std::uint8_t) override;
        int SendForAnalysis(const void*,int) override;
        int ReceiveForAnalysis(void*,int) override;
        bool AddToSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected foreign call");}
        bool RemoveFromSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected foreign call");}
        bool ClearSetForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected foreign call");}
        bool SelectForAnalysis(std::uint32_t) override{throw std::logic_error("Unexpected foreign call");}
        int SendToForAnalysis(std::uint32_t,std::uint16_t,const void*,int) override{throw std::logic_error("Unexpected foreign call");}
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void*,int) override{throw std::logic_error("Unexpected foreign call");}
        bool GetLocalAddressForAnalysis(std::uint32_t&) override{throw std::logic_error("Unexpected foreign call");}
    };
    struct Host final:spSocketStreamHost
    {
        std::string mode;std::vector<std::string> events;unsigned created=0,live=0,receives=0;
        std::vector<int> chunks{0};int sendResult=5;spNetworkAcceptForAnalysis incoming;
        std::unique_ptr<spNetwork> CreateNetwork() override
        {++live;events.push_back("create-network:"+std::to_string(++created));return std::make_unique<Network>(*this,created);}
        void ReportInvalidNetwork() override{events.push_back("invalid-network");}
        bool Operation(const std::string& name,const std::string& detail={})
        {events.push_back(name+detail);return mode!="fail-"+name;}
    };
    Network::~Network(){h.events.push_back("delete-network:"+std::to_string(id));--h.live;}
    bool Network::OpenForAnalysis(std::uint32_t t){return h.Operation("open",":"+std::to_string(t));}
    bool Network::ConnectForAnalysis(std::uint32_t a,std::uint16_t p){return h.Operation("connect",":"+std::to_string(a)+":"+std::to_string(p));}
    bool Network::BindForAnalysis(std::uint32_t a,std::uint16_t p){return h.Operation("bind",":"+std::to_string(a)+":"+std::to_string(p));}
    bool Network::ListenForAnalysis(){return h.Operation("listen");}
    bool Network::CloseForAnalysis(){return h.Operation("close");}
    bool Network::SetBlockingForAnalysis(std::uint8_t b){return h.Operation("blocking",":"+std::to_string(b));}
    spNetworkAcceptForAnalysis Network::AcceptForAnalysis(){h.events.push_back("accept");return std::move(h.incoming);}
    int Network::SendForAnalysis(const void* p,int size)
    {
        Check(size>=0&&size<=2000,"bounded foreign send input");
        h.events.push_back("send:"+std::to_string(size)+":"+Hex(p,size)+":"+std::to_string(h.sendResult));return h.sendResult;
    }
    int Network::ReceiveForAnalysis(void* p,int size)
    {
        Check(size==500&&h.receives<h.chunks.size(),"bounded receive inputs");
        const int result=h.chunks[h.receives++];
        if(result>0){const auto bytes=Payload(result);std::copy(bytes.begin(),bytes.end(),static_cast<std::uint8_t*>(p));}
        h.events.push_back("receive:500:"+std::to_string(result));return result;
    }
    // Test-only stream boundaries wrap the common MemoryStream implementation.
    struct Stream final:spStream
    {
        Host& h;spMemoryStream memory;unsigned writes=0,failWrite=0;bool sending=false;
        explicit Stream(Host& owner):h(owner){Check(memory.Open("buffer"),"memory open");}
        bool Open(const char* n) override{return memory.Open(n);}
        bool Open(std::uint32_t m,const char* n) override{return memory.Open(m,n);}
        bool Close() override{return memory.Close();}
        bool Seek(SeekSource s,std::int32_t o) override{return memory.Seek(s,o);}
        bool GetCurrentPosition(std::uint32_t& p) const override{return memory.GetCurrentPosition(p);}
        bool ReadData(void* p,std::uint32_t n) override{return memory.ReadData(p,n);}
        bool WriteData(const void* p,std::uint32_t n) override
        {
            ++writes;h.events.push_back("write:"+std::to_string(n)+":"+Hex(p,n));
            return writes==failWrite?false:memory.WriteData(p,n);
        }
        bool vfunc_WriteFromStream(spStream* p,std::uint32_t n) override{return memory.vfunc_WriteFromStream(p,n);}
        bool GetSize(std::uint32_t* p) const override
        {
            if(sending)h.events.push_back("get-size");const bool ok=memory.GetSize(p);
            return sending&&h.mode=="stream-size-false"?false:ok;
        }
        void* GetBuffer() noexcept override{if(sending)h.events.push_back("get-buffer");return memory.GetBuffer();}
    };
    const char* Modes[]={"defaults","clone","copy","leaves","bind-tcp","bind-udp","connect","mode-zero","mode-three","mode-wrap","fail-open","fail-bind","fail-connect","fail-listen","fail-blocking","close","fail-close","send-error","send-zero","send-short","send-negative","receive-zero","receive-error","receive-short","receive-499","receive-full-zero","receive-full-short","receive-two-full","receive-late-error","receive-write-fail","receive-second-write-fail","receive-zero-write-fail","stream-send","stream-send-error","stream-size-false","adopt","adopt-error","accept-null","accept-object","accept-error"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known case");
        auto h=std::make_shared<Host>();h->mode=mode;SetSocketStreamFactoryHostForAnalysis(h);
        auto s=std::make_unique<spSocketStream>();spCloneManager manager;
        Check(s->IsExactly(spSocketStream::ClassID)&&s->IsKindOf(spStream::ClassID),"socket RTTI");
        std::vector<std::string> states;std::vector<std::uint32_t> results;
        auto State=[&](spSocketStream& x)
        {
            std::ostringstream o;o<<'['<<x.TypeForAnalysis()<<','<<x.AddressForAnalysis()<<','<<x.PortWordForAnalysis()<<','<<unsigned(x.BlockingForAnalysis())<<','<<static_cast<Network&>(x.NetworkForAnalysis()).id<<','<<x.GetLogicalOriginForAnalysis()<<','<<int(x.GetStreamName()!=nullptr)<<']';states.push_back(o.str());
        };
        auto Configure=[&](){s->SetEndpointForAnalysis(mode=="bind-udp"?0:1,0x12345678,0xfedc,255);};
        State(*s);
        if(mode=="clone"||mode=="copy")
        {
            Configure();s->SetLogicalOriginForAnalysis(0x1234);
            auto other=mode=="clone"?s->vfunc_10(manager):std::make_unique<spSocketStream>();
            auto& socket=static_cast<spSocketStream&>(*other);
            if(mode=="copy"){socket.SetEndpointForAnalysis(7,0xffffffff,0xffff,0);results.push_back(s->vfunc_14(socket,manager));}
            else results.push_back(manager.FindClone(*s)==other.get());
            State(*s);State(socket);other.reset();
        }
        else if(mode=="leaves")
        {
            std::uint32_t value=0x12345678;std::uint8_t data[5]{};
            results={unsigned(s->Open(nullptr)),unsigned(s->Seek(static_cast<spStream::SeekSource>(7),-1)),unsigned(s->GetCurrentPosition(value)),unsigned(s->ReadData(data,5)),unsigned(s->vfunc_WriteFromStream(nullptr,9)),unsigned(s->GetSize(&value)),unsigned(s->GetBuffer()!=nullptr),value};State(*s);
            Check(value==0x12345678,"successful leaves preserve output");
        }
        else if(mode.rfind("receive-",0)==0)
        {
            if(mode=="receive-error")h->chunks={-1};else if(mode=="receive-short")h->chunks={3};else if(mode=="receive-499")h->chunks={499};
            else if(mode=="receive-full-zero")h->chunks={500,0};else if(mode=="receive-full-short")h->chunks={500,3};else if(mode=="receive-two-full")h->chunks={500,500,499};else if(mode=="receive-late-error")h->chunks={500,-1};else if(mode=="receive-write-fail")h->chunks={500};else if(mode=="receive-second-write-fail")h->chunks={500,3};
            Stream buffer(*h);buffer.failWrite=(mode=="receive-write-fail"||mode=="receive-zero-write-fail")?1:mode=="receive-second-write-fail"?2:0;
            results.push_back(s->ReceiveStreamForAnalysis(buffer));std::uint32_t size=0,position=0;
            Check(buffer.memory.GetSize(&size)&&buffer.memory.GetCurrentPosition(position),"memory receive state");
            states.push_back('['+std::to_string(size)+','+std::to_string(position)+','+Quote(Hex(buffer.memory.GetBuffer(),size))+']');
        }
        else if(mode.rfind("stream-",0)==0)
        {
            Stream buffer(*h);const auto data=Payload(5);Check(buffer.memory.WriteData(data.data(),5),"source payload");buffer.sending=true;
            if(mode=="stream-send-error")h->sendResult=-1;
            results.push_back(static_cast<std::uint32_t>(s->SendStreamForAnalysis(buffer)));
        }
        else if(mode.rfind("send-",0)==0)
        {
            h->sendResult=mode=="send-error"?-1:mode=="send-zero"?0:mode=="send-short"?2:-2;
            const auto data=Payload(5);results.push_back(s->WriteData(data.data(),5));
        }
        else if(mode=="close"||mode=="fail-close")results.push_back(s->Close());
        else if(mode=="adopt"||mode=="accept-object")
        {
            auto network=h->CreateNetwork();static_cast<Network&>(*network).Fields();
            if(mode=="adopt")
            {s->SetEndpointForAnalysis(1,0xffffffff,0xffff,255);results.push_back(s->AdoptNetworkForAnalysis({std::move(network),0}));State(*s);}
            else
            {h->incoming.object=std::move(network);auto other=s->AcceptForAnalysis();results.push_back(bool(other));State(*other);other.reset();}
        }
        else if(mode=="adopt-error"||mode=="accept-error")
        {
            if(mode=="adopt-error"){results.push_back(s->AdoptNetworkForAnalysis({nullptr,0xffffffff}));State(*s);}
            else{h->incoming.failureWord=0xffffffff;auto other=s->AcceptForAnalysis();results.push_back(bool(other));State(*other);other.reset();}
        }
        else if(mode=="accept-null")results.push_back(s->AcceptForAnalysis()==nullptr);
        else if(mode!="defaults")
        {
            Configure();const auto m=mode=="mode-zero"?0u:mode=="mode-three"?3u:mode=="mode-wrap"?0xffffffffu:(mode=="connect"||mode=="fail-connect")?2u:1u;
            results.push_back(s->Open(m,nullptr));State(*s);
        }
        s.reset();Check(h->live==0,"all foreign networks released");SetSocketStreamFactoryHostForAnalysis(nullptr);
        std::ostringstream out;out<<"{\"results\":[";
        for(unsigned i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"],\"states\":[";
        for(unsigned i=0;i<states.size();++i){if(i)out<<',';out<<states[i];}out<<"],\"events\":[";
        for(unsigned i=0;i<h->events.size();++i){if(i)out<<',';out<<Quote(h->events[i]);}out<<"]}";return out.str();
    }
    void MissingHost()
    {
        SetSocketStreamFactoryHostForAnalysis(nullptr);bool rejected=false;
        try{spSocketStream stream;}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"missing host must not silently succeed");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:Modes)(void)Run(mode);MissingHost();
        std::cout<<"PASS "<<std::size(Modes)<<" SocketStream operation cases\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
