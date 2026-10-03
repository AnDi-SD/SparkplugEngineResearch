#include "Code/SparkplugDX/spDXNetwork.h"
#include "Analysis/PC/spDXNetworkAbi.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    std::uint32_t Swap32(std::uint32_t x){return (x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24);}
    std::uint16_t Swap16(std::uint16_t x){return static_cast<std::uint16_t>((x>>8)|(x<<8));}
    std::string Address(const spNetworkAddressForAnalysis& a)
    {
        std::string s;const char* hex="0123456789abcdef";
        for(unsigned i=0;i<16;++i){auto b=(a.knownBytes&(1u<<i))?a.bytes[i]:0xcc;s+=hex[b>>4];s+=hex[b&15];}
        return s;
    }
    struct Host final : spDXNetworkHost
    {
        std::vector<std::string> events;
        std::uint32_t socketResult=41,acceptResult=73,ip=0x04030201,acceptLength=16;
        int status=0,ioResult=3,error=10035,selectResult=1;
        bool sockNameWrites=true;
        template<class... T> void Event(const char* name,const T&... values)
        {std::ostringstream s;s<<name;((s<<':'<<values),...);events.push_back(s.str());}
        int Startup(std::uint16_t v) override{Event("startup",v);return 10091;}
        int Cleanup() noexcept override{events.push_back("cleanup");return -1;}
        std::uint32_t Socket(int f,int t,int p) override{Event("socket",f,t,p);return socketResult;}
        std::uint16_t Htons(std::uint16_t v) override{Event("htons",v);return Swap16(v);}
        std::uint32_t Htonl(std::uint32_t v) override{Event("htonl",v);return Swap32(v);}
        std::uint16_t Ntohs(std::uint16_t v) override{Event("ntohs",v);return Swap16(v);}
        std::uint32_t Ntohl(std::uint32_t v) override{Event("ntohl",v);return Swap32(v);}
        int Connect(std::uint32_t s,const spNetworkAddressForAnalysis& a) override{Event("connect",s,Address(a));return status;}
        int Bind(std::uint32_t s,const spNetworkAddressForAnalysis& a) override{Event("bind",s,Address(a));return status;}
        int GetSockName(std::uint32_t s,spNetworkAddressForAnalysis& a,std::uint32_t& n) override
        {Event("getsockname",s,n);if(sockNameWrites){a.Clear();a.Put16(0,2);a.Put16(2,0x3412);a.Put32(4,0x04030201);n=16;}return sockNameWrites?0:-1;}
        int Listen(std::uint32_t s,int b) override{Event("listen",s,b);return status;}
        int CloseSocket(std::uint32_t s) noexcept override{Event("close",s);return -1;}
        int IoctlSocket(std::uint32_t s,std::uint32_t c,std::uint32_t& v) override{Event("ioctl",s,c,v);return status;}
        int Send(std::uint32_t s,const void*,int n,int f) override{Event("send",s,n,f);return ioResult;}
        int SendTo(std::uint32_t s,const void*,int n,int f,const spNetworkAddressForAnalysis& a) override{Event("sendto",s,n,f,Address(a));return ioResult;}
        int Recv(std::uint32_t s,void* data,int n,int f) override
        {Event("recv",s,n,f);if(ioResult>0&&n>=ioResult)std::memset(data,0x5a,ioResult);return ioResult;}
        int RecvFrom(std::uint32_t s,void* data,int n,int f,spNetworkAddressForAnalysis& a,std::uint32_t& len) override
        {Event("recvfrom",s,n,f,len);if(ioResult>=0){a.Clear();a.Put16(0,2);a.Put16(2,0x7856);a.Put32(4,0x08070605);len=16;}if(ioResult>0&&n>=ioResult)std::memset(data,0x5a,ioResult);return ioResult;}
        std::uint32_t Accept(std::uint32_t s,spNetworkAddressForAnalysis& a,std::uint32_t& len) override
        {Event("accept",s,len);if(acceptResult!=0xffffffff){a.Clear();a.Put16(0,2);a.Put16(2,0x3412);a.Put32(4,0x04030201);len=acceptLength;}return acceptResult;}
        int LastError() override{Event("error");return error;}
        int Select(int n,spNetworkSocketSetForAnalysis& r,spNetworkSocketSetForAnalysis& w,spNetworkSocketSetForAnalysis& e,std::int32_t s,std::int32_t u) override
        {Event("select",n,r.count,w.count,e.count,s,u);if(selectResult>=0){r.count=0;w.count=0;e.count=0;}return selectResult;}
        int GetHostName(std::optional<std::string>& n,int cap) override{Event("hostname",cap);n="fixture";return 0;}
        std::uint32_t GetHostByName(const std::string& n) override{Event("hostbyname",n);return ip;}
    };
    std::string Snapshot(const spDXNetwork& n)
    {
        std::ostringstream s;s<<'['<<unsigned(n.IsConnectedForAnalysis())<<','<<n.GetTypeForAnalysis()<<','<<n.GetAddressForAnalysis()<<','<<n.GetPortForAnalysis()<<','<<n.GetSocketForAnalysis()<<",\""<<Address(n.GetSockAddressForAnalysis())<<"\",";
        if(n.GetName())s<<'"'<<n.GetName()<<'"';else s<<"null";s<<']';return s.str();
    }
    const std::vector<std::string> Modes={"factory","clone","copy","open-udp","open-tcp","open-unknown","open-fail","open-existing","connect-ok","connect-fail","connect-invalid","bind-ok","bind-fail","bind-namefail","bind-invalid","listen-ok","listen-fail","listen-invalid","blocking-zero","blocking-nonzero","blocking-fail","blocking-invalid","send-ok","send-fail","send-invalid","sendto-ok","sendto-fail","sendto-invalid","recv-ok","recv-fail","recv-invalid","recvfrom-ok","recvfrom-fail","recvfrom-invalid","accept-ok","accept-short","accept-wouldblock","accept-error","accept-invalid","sets","select-ready","select-zero","select-error","select-overflow","local","local-cache","close-invalid","teardown-connected","teardown-unconnected"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(Modes.begin(),Modes.end(),mode)!=Modes.end(),"Unknown network case");
        auto h=std::make_shared<Host>();auto object=std::make_unique<spDXNetwork>(h);auto& n=*object;
        Check(n.IsKindOf(spNetwork::ClassID)&&n.IsExactly(spDXNetwork::ClassID),"network RTTI");
        std::vector<std::uint32_t> results;std::vector<std::string> states,texts;
        std::array<unsigned char,8> data{};
        auto Result=[&](auto v){results.push_back(static_cast<std::uint32_t>(v));};
        auto State=[&](const spDXNetwork& v){states.push_back(Snapshot(v));};
        if(mode=="factory")State(n);
        else if(mode=="clone"||mode=="copy")
        {
            n.SetName("network");Result(n.OpenForAnalysis(0));Result(n.ConnectForAnalysis(0x01020304,0x1234));
            if(mode=="clone")
            {auto clone=n.Clone();auto* c=dynamic_cast<spDXNetwork*>(clone.get());Check(c&&c->GetName()&&c->GetSocketForAnalysis()==0xffffffff,"clone name and fresh socket");State(*c);}
            else
            {spDXNetwork target(h);Result(target.OpenForAnalysis(1));spCloneManager manager;Result(n.vfunc_14(target,manager));State(target);Result(target.CloseForAnalysis());}
            State(n);Result(n.CloseForAnalysis());
        }
        else if(mode.rfind("open-",0)==0)
        {
            if(mode=="open-fail")h->socketResult=0xffffffff;
            if(mode=="open-existing")Result(n.OpenForAnalysis(1));
            Result(n.OpenForAnalysis(mode=="open-tcp"?1:mode=="open-unknown"||mode=="open-existing"?99:0));State(n);Result(n.CloseForAnalysis());
        }
        else if(mode=="sets")
        {
            Result(n.AddToSetForAnalysis(0));Result(n.OpenForAnalysis(1));
            for(unsigned i=0;i<66;++i)Result(n.AddToSetForAnalysis(0));
            Result(n.AddToSetForAnalysis(1));Result(n.AddToSetForAnalysis(2));Result(n.AddToSetForAnalysis(3));
            Result(n.RemoveFromSetForAnalysis(0));Result(n.RemoveFromSetForAnalysis(3));
            Result(n.ClearSetForAnalysis(1));Result(n.ClearSetForAnalysis(3));State(n);Result(n.CloseForAnalysis());
        }
        else if(mode.rfind("select-",0)==0)
        {
            Result(n.AddToSetForAnalysis(0));Result(n.AddToSetForAnalysis(1));Result(n.AddToSetForAnalysis(2));
            h->selectResult=mode=="select-zero"?0:mode=="select-error"?-1:1;
            Result(n.SelectForAnalysis(mode=="select-overflow"?0xffffffff:250));State(n);
        }
        else if(mode=="local"||mode=="local-cache")
        {
            std::uint32_t a;Result(n.GetLocalAddressForAnalysis(a));Result(a);texts.push_back(n.GetLocalAddressTextForAnalysis());
            if(mode=="local-cache"){h->ip=0x08070605;Result(n.GetLocalAddressForAnalysis(a));Result(a);texts.push_back(n.GetLocalAddressTextForAnalysis());}
            State(n);
        }
        else if(mode=="close-invalid"){Result(n.CloseForAnalysis());State(n);}
        else
        {
            const bool invalid=mode.find("invalid")!=std::string::npos;
            if(!invalid)Result(n.OpenForAnalysis(1));
            if(mode.find("fail")!=std::string::npos){h->status=-1;h->ioResult=-1;}
            if(mode.rfind("connect-",0)==0||mode=="teardown-connected")Result(n.ConnectForAnalysis(0x01020304,0x1234));
            else if(mode.rfind("bind-",0)==0)
            {h->sockNameWrites=mode!="bind-namefail";Result(n.BindForAnalysis(0xffffffff,0));}
            else if(mode.rfind("listen-",0)==0)Result(n.ListenForAnalysis());
            else if(mode.rfind("blocking-",0)==0)Result(n.SetBlockingForAnalysis(mode=="blocking-zero"?0:255));
            else if(mode.rfind("sendto-",0)==0)Result(n.SendToForAnalysis(0x05060708,0x5678,data.data(),8));
            else if(mode.rfind("send-",0)==0)Result(n.SendForAnalysis(data.data(),8));
            else if(mode.rfind("recvfrom-",0)==0)Result(n.ReceiveFromForAnalysis(0x12345678,0xabcdef01,data.data(),8));
            else if(mode.rfind("recv-",0)==0)Result(n.ReceiveForAnalysis(data.data(),8));
            else if(mode.rfind("accept-",0)==0)
            {
                if(mode=="accept-wouldblock"||mode=="accept-error")h->acceptResult=0xffffffff;
                if(mode=="accept-error")h->error=10054;
                if(mode=="accept-short")h->acceptLength=8;
                auto accepted=n.AcceptForAnalysis();Result(accepted.object!=nullptr);Result(accepted.failureWord);
                if(accepted.object){auto* c=dynamic_cast<spDXNetwork*>(accepted.object.get());Check(c!=nullptr,"accepted network");State(*c);}
            }
            State(n);
            if(mode.rfind("teardown-",0)!=0)Result(n.CloseForAnalysis());
        }
        object.reset();Check(h->globals.instanceCount==0,"complete lifetime instance balance");
        std::ostringstream out;out<<"{\"results\":[";
        for(unsigned i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"],\"states\":[";
        for(unsigned i=0;i<states.size();++i){if(i)out<<',';out<<states[i];}out<<"],\"events\":[";
        for(unsigned i=0;i<h->events.size();++i){if(i)out<<',';out<<'"'<<h->events[i]<<'"';}out<<"],\"texts\":[";
        for(unsigned i=0;i<texts.size();++i){if(i)out<<',';out<<'"'<<texts[i]<<'"';}out<<"],\"sets\":[";
        for(unsigned i=0;i<3;++i){if(i)out<<',';out<<'['<<h->globals.sets[i].count;for(auto s:h->globals.sets[i].sockets)out<<','<<s;out<<']';}
        out<<"],\"data\":[";for(unsigned i=0;i<data.size();++i){if(i)out<<',';out<<unsigned(data[i]);}out<<"]}";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        bool missingHost=false;try{spDXNetwork n;}catch(const std::logic_error&){missingHost=true;}Check(missingHost,"missing service must fail");
        Check(spNetwork::StaticRTTI().factory==nullptr,"native base has no factory");
        auto host=std::make_shared<Host>();SetDXNetworkFactoryHostForAnalysis(host);
        auto created=spRTTIManager::Instance().Create(spDXNetwork::ClassID);Check(dynamic_cast<spDXNetwork*>(created.get())!=nullptr,"RTTI factory");
        auto* network=dynamic_cast<spDXNetwork*>(created.get());spCloneManager manager;
        Check(network->spNetwork::vfunc_10(manager)==nullptr,"native base null clone");
        Check(network->spNetwork::GetLocalAddressTextForAnalysis()==nullptr,"native base primary40 null stub");
        SetDXNetworkFactoryHostForAnalysis(nullptr);created.reset();Check(host->globals.instanceCount==0,"factory host retained through teardown");
        for(const auto& mode:Modes)(void)Run(mode);
        std::cout<<"PASS "<<Modes.size()<<" network behavior/lifetime cases\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
