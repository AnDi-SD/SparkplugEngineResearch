#include "Code/Sparkplug/spNetworkManager.h"
#include "Analysis/PC/spNetworkManagerAbi.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
    std::string Quote(const std::string& s)
    {
        std::string out="\"";for(char c:s){if(c=='"'||c=='\\')out+='\\';out+=c;}return out+'"';
    }
    struct Host final:spNetworkManagerHost
    {
        std::vector<std::string> events;
        std::map<Handle,std::string> services;
        std::map<Handle,int> locks;
        std::map<Handle,Handle> controllerLocks;
        std::map<Handle,spNetworkManagerPacketForAnalysis> packets;
        Handle next=1;
        std::int32_t sendResult=1;
        bool failOpen=false,failPeer=false;
        std::uint32_t phase=0,ticks=0;
        std::uint8_t active=0;
        std::vector<Handle> children;
        std::map<Handle,std::pair<std::uint32_t,std::uint32_t>> childKeys;
        std::map<std::uint32_t,bool> changed,pressed;
        unsigned kind=0;
        static std::string Name(Service s)
        {
            switch(s){case Service::Debug:return "debug";case Service::Server:return "server";
            case Service::Peer:return "peer";case Service::Controller:return "controller";
            case Service::Timer:return "timer";case Service::Matchmaking:return "matchmaking";}throw std::runtime_error("service");
        }
        Handle NewLock(const std::string& name){Handle h=next++;locks[h]=0;services[h]=name;return h;}
        Handle CreateQueueLock() override{return NewLock("queue");}
        Handle CreateErrorLock() override{events.push_back("create:error-lock");return NewLock("error-lock");}
        void FreeLockStorage(Handle h) noexcept override{if(locks[h]!=0)std::terminate();locks.erase(h);services.erase(h);}
        void EnterLock(Handle h) override{Check(h&&locks.count(h),"known lock");++locks[h];events.push_back("enter:"+services[h]);}
        void LeaveLock(Handle h) noexcept override{if(!locks.count(h)||locks[h]<=0)std::terminate();--locks[h];events.push_back("leave:"+services[h]);}
        Handle CreateService(Service s,spNetworkManager&) override
        {
            Handle h=next++;services[h]=Name(s);events.push_back("create:"+Name(s));
            if(s==Service::Controller)controllerLocks[h]=NewLock("controller-lock");return h;
        }
        void DeleteService(Service s,Handle h) noexcept override
        {
            if(services[h]!=Name(s))std::terminate();events.push_back("delete:"+Name(s));services.erase(h);
            if(s==Service::Controller){FreeLockStorage(controllerLocks[h]);controllerLocks.erase(h);}
        }
        void StartDebug(Handle) override{events.push_back("start:debug");}
        void SetDebugFile(Handle,std::uint8_t v,const std::string& path) override{events.push_back("file:"+std::to_string(v)+":"+path);}
        void SetServerQueue(Handle,spNetworkManager&) override{events.push_back("server-queue");}
        void SetPeerId(Handle,std::uint16_t id) override{Check(id==0,"initial peer id");}
        void Transition(Handle,std::uint32_t a,std::uint32_t b) override{events.push_back("transition:"+std::to_string(a)+":"+std::to_string(b));}
        void PumpController(Handle) override{events.push_back("controller-pump");}
        Handle ControllerLock(Handle h) override{return controllerLocks.at(h);}
        std::uint32_t ControllerState(Handle) override{return phase;}
        void PumpMatchmaking(Handle) override{events.push_back("matchmaking-pump");}
        Timer ReadTimer(Handle) override{return {active,ticks,1};}
        void StartTimer(Handle) override{events.push_back("timer-start");}
        void RefreshTimer(Handle) override{events.push_back("timer-refresh");}
        void SetPeerTimestamp(Handle,std::uint32_t t) override{Check(t==ticks,"peer timestamp");}
        std::uint16_t ServerId(Handle) override{return 0;}
        bool OpenServer(Handle) override{events.push_back("server-open");return !failOpen;}
        bool ConnectPeer(Handle,std::uint32_t v) override{events.push_back("peer-connect:"+std::to_string(v));return !failPeer;}
        void SetServerPeer(Handle,Handle) override{events.push_back("server-peer");}
        std::int32_t SendPeer(Handle,std::uint16_t dst,std::uint16_t type,const std::vector<std::uint8_t>& data,std::uint8_t tcp,std::uint32_t a,std::uint32_t b,std::uint32_t c) override
        {
            std::ostringstream o;o<<"peer-send:"<<dst<<':'<<type<<':'<<data.size()<<':'<<unsigned(tcp)<<':'<<a<<':'<<b<<':'<<c<<":[";
            for(std::size_t i=0;i<data.size();++i){if(i)o<<", ";o<<unsigned(data[i]);}o<<']';events.push_back(o.str());return sendResult;
        }
        const spNetworkManagerPacketForAnalysis& ReadPacket(Handle h) override{return packets.at(h);}
        void DeletePacket(Handle h) noexcept override{events.push_back("delete:packet");packets.erase(h);}
        void ForwardNotification(spNetworkManager&,const void* n) override{events.push_back("forward-notification:"+std::to_string(*static_cast<const std::uint32_t*>(n)));}
        void DispatchError(spNetworkManager& m,const spNetworkManagerNotificationForAnalysis& n,const std::string& text) override
        {Check(n.code==35&&n.sender==&m&&n.word0C==1&&n.word04==0&&n.word08==0&&n.word14==0&&n.word1C==reinterpret_cast<std::uintptr_t>(text.c_str()),"complete error notification");events.push_back("error-notification:"+std::to_string(n.word18)+":"+text);}
        void Log(Handle,const std::string& category,std::int32_t level,const std::string& format,const std::vector<std::uint32_t>& args) override
        {Check(category=="Network Manager","category");if(format.find("%d")!=std::string::npos)Check(args.size()==2,"format arguments");events.push_back("log:"+std::to_string(level)+":"+format);}
        std::vector<Handle> Children(spNetworkManager&) override{return children;}
        std::uint32_t ChildClassId(Handle h) override{return childKeys.at(h).first;}
        std::uint32_t ChildField2C(Handle h) override{return childKeys.at(h).second;}
        void Notify(Handle,const spNetworkManagerNotificationForAnalysis& n) override{events.push_back("notify:"+std::to_string(n.code)+":"+std::to_string(n.word18)+":"+std::to_string(n.word1C));}
        bool InputChanged(Handle,std::uint32_t k) override{return changed[k];}
        bool InputPressed(Handle,std::uint32_t k) override{return pressed[k];}
        Handle Packet(std::uint16_t type,std::vector<std::uint8_t> data={},std::uint32_t a=0,std::uint32_t b=0)
        {Handle h=next++;packets[h]={type,a,b,std::move(data)};return h;}
    };
    std::string Snapshot(const spNetworkManager& manager)
    {
        const auto s=manager.GetStateForAnalysis();std::ostringstream o;
        o<<"{\"flags\":["<<unsigned(s.started)<<','<<unsigned(s.word15)<<','<<unsigned(s.forwardNotifications)<<"],\"error\":"<<s.error<<",\"id\":"<<s.uniqueId<<",\"queue\":"<<s.queued<<",\"bias\":"<<s.bias<<",\"stats\":[";
        for(std::size_t i=0;i<s.statistics.size();++i){if(i)o<<',';o<<s.statistics[i];}o<<"],\"services\":[";
        for(unsigned i=0;i<8;++i){if(i)o<<',';o<<int(s.services[i]);}o<<"]}";return o.str();
    }
    const char* Modes[]={"defaults","start","start-twice","stop","stop-twice","restart","copy","clone","notify-enabled","notify-disabled",
        "error-0","error-1","error-2","error-3","error-4","error-5","error-6","error-7","error-8","error-9","error-10","error-2147483647",
        "error-0-lock","error-3-lock","error-10-lock","shutdown-queue","pump-empty","pump-disabled","pump-unknown","pump-25","pump-26",
        "handshake-short","handshake-id","handshake-other","probe","probe-zero","probe-negative","hello-request","hello-request-zero","hello-request-negative",
        "hello-reply","hello-reply-invalid","hello-reply-zero-latency","hello-reply-series","connect","connect-open-failure","connect-peer-failure","connect-probe-zero","timeout-equal","timeout-expired",
        "recipient-first","recipient-same","recipient-replace","recipient-null","input-default","input-shift40","input-shift52","input-disabled","broadcast-full","broadcast-empty",
        "lazy-log","singleton-delete-old","shutdown-null"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known case");
        auto h=std::make_shared<Host>();SetNetworkManagerFactoryHostForAnalysis(h);spNetworkManager::SetLatencySamplesForAnalysis({},0);
        auto source=std::make_unique<spNetworkManager>();spCloneManager clones;
        std::vector<std::string> states;std::vector<unsigned> results;
        auto state=[&](const spNetworkManager& m){states.push_back(Snapshot(m));};
        auto start=[&](){results.push_back(source->StartForAnalysis());};
        auto enqueue=[&](std::vector<Host::Handle> packets){auto n=h->events.size();for(auto p:packets)source->EnqueueForAnalysis(p);h->events.resize(n);};
        if(mode=="defaults")state(*source);
        else if(mode=="singleton-delete-old")
        {auto second=std::make_unique<spNetworkManager>();source.reset();results.push_back(spNetworkManager::CurrentForAnalysis()==nullptr);state(*second);second.reset();}
        else if(mode=="lazy-log")
        {
            start();auto second=source->vfunc_10(clones);second.reset();Check(!spNetworkManager::CurrentForAnalysis(),"clone deletion clears singleton");
            results.push_back(source->SendHelloRequestForAnalysis());results.push_back(spNetworkManager::CurrentForAnalysis()!=source.get());state(*source);spNetworkManager::ReleaseLazyStorageForAnalysis();
        }
        else if(mode.rfind("error-",0)==0)
        {if(mode.find("-lock")!=std::string::npos)start();source->SetErrorForAnalysis(std::stoi(mode.substr(6)));state(*source);}
        else if(mode=="start"||mode=="start-twice"||mode=="stop"||mode=="stop-twice"||mode=="restart")
        {
            start();state(*source);if(mode=="start-twice"){start();state(*source);}
            if(mode=="stop"||mode=="stop-twice"||mode=="restart")
            {source->StopForAnalysis();state(*source);if(mode=="stop-twice"){source->StopForAnalysis();state(*source);}if(mode=="restart"){start();state(*source);}}
        }
        else if(mode=="copy"||mode=="clone")
        {
            start();source->SetControlsForAnalysis(255,127,254);source->SetErrorForAnalysis(7);h->events.resize(h->events.size()-3);
            auto target=mode=="clone"?source->vfunc_10(clones):std::make_unique<spNetworkManager>();
            results.push_back(mode=="clone"?clones.FindClone(*source)==target.get():source->vfunc_14(*target,clones));
            state(*source);state(static_cast<const spNetworkManager&>(*target));target.reset();
        }
        else if(mode.rfind("notify-",0)==0)
        {if(mode=="notify-enabled")source->SetControlsForAnalysis(0,0,255);std::uint32_t n=26;source->vfunc_0C(&n);state(*source);}
        else if(mode.rfind("recipient-",0)==0)
        {source->SetRecipientForAnalysis(1000);if(mode!="recipient-first")source->SetRecipientForAnalysis(mode=="recipient-same"?1000:mode=="recipient-replace"?1001:0);state(*source);}
        else
        {
            start();source->SetControlsForAnalysis(1,0,1);
            if(mode=="pump-disabled")source->SetControlsForAnalysis(1,0,0);
            if(mode=="shutdown-queue"||mode=="shutdown-null")
            {auto a=h->Packet(1),b=h->Packet(9);enqueue(mode=="shutdown-null"?std::vector<Host::Handle>{a,0,b}:std::vector<Host::Handle>{a,b});source->StopForAnalysis();state(*source);}
            else if(mode.rfind("input-",0)==0)
            {
                if(mode!="input-disabled")source->SetRecipientForAnalysis(1000);
                for(auto key:{0u,1u,13u,15u,29u,99u})h->changed[key]=true;for(auto key:{0u,1u,15u})h->pressed[key]=true;
                if(mode=="input-shift40")h->pressed[40]=true;if(mode=="input-shift52")h->pressed[52]=true;
                results.push_back(source->DispatchInputForAnalysis());state(*source);
            }
            else if(mode.rfind("broadcast-",0)==0)
            {
                if(mode=="broadcast-full"){h->children={1000,1001,1002};h->childKeys={{1000,{0x1234,0x55}},{1001,{0x1234,0x99}},{1002,{0x1234,0x55}}};}
                enqueue({h->Packet(2,{},0x1234,0x55)});results.push_back(source->PumpForAnalysis());state(*source);
            }
            else if(mode.rfind("probe",0)==0||mode.rfind("hello-request",0)==0||mode.rfind("hello-reply",0)==0||mode.rfind("connect",0)==0)
            {
                if(mode.size()>=5&&mode.substr(mode.size()-5)=="-zero")h->sendResult=0;
                if(mode.size()>=9&&mode.substr(mode.size()-9)=="-negative")h->sendResult=-1;
                if(mode.rfind("probe",0)==0)results.push_back(source->SendProbeForAnalysis());
                else if(mode.rfind("hello-request",0)==0)results.push_back(source->SendHelloRequestForAnalysis());
                else if(mode.rfind("connect",0)==0)
                {h->failOpen=mode=="connect-open-failure";h->failPeer=mode=="connect-peer-failure";results.push_back(source->StartConnectionForAnalysis());}
                else
                {
                    auto p=h->Packet(6,{std::uint8_t(mode=="hello-reply-invalid"?0:100),0,0,0,200,0,0,0});source->SetCurrentPacketForAnalysis(p);
                    h->ticks=mode=="hello-reply-zero-latency"?100:300;
                    if(mode=="hello-reply-series")for(unsigned i=0;i<7;++i){h->packets[p].data[0]=std::uint8_t(100+i*10);h->ticks=300+i*40;results.push_back(source->HandleHelloReplyForAnalysis());state(*source);}
                    else results.push_back(source->HandleHelloReplyForAnalysis());
                    h->DeletePacket(p);source->SetCurrentPacketForAnalysis(0);
                }
                if(mode!="hello-reply-series")state(*source);
            }
            else
            {
                if(mode=="pump-unknown"||mode=="pump-25"||mode=="pump-26")
                {std::vector<Host::Handle> p;for(int i=0;i<(mode=="pump-25"?25:mode=="pump-26"?26:1);++i)p.push_back(h->Packet(99));enqueue(p);}
                if(mode.rfind("handshake-",0)==0)
                {std::vector<std::uint8_t> data{std::uint8_t(mode=="handshake-other"?2:1),0,0x34,0x12};if(mode=="handshake-short")data.pop_back();enqueue({h->Packet(4,data)});}
                if(mode.rfind("timeout",0)==0){h->phase=3;source->SetStatisticForAnalysis(5,2);h->ticks=mode=="timeout-equal"?10000:10001;}
                results.push_back(source->PumpForAnalysis());state(*source);
            }
        }
        source.reset();Check(h->locks.empty()&&h->services.empty(),"all service and lock owners released");
        std::ostringstream o;o<<"{\"results\":[";for(std::size_t i=0;i<results.size();++i){if(i)o<<',';o<<results[i];}
        o<<"],\"states\":[";for(std::size_t i=0;i<states.size();++i){if(i)o<<',';o<<states[i];}
        o<<"],\"events\":[";for(std::size_t i=0;i<h->events.size();++i){if(i)o<<',';o<<Quote(h->events[i]);}
        o<<"],\"remaining\":[";for(std::size_t i=0;i<h->packets.size();++i){if(i)o<<',';o<<"\"packet\"";}o<<"]}";return o.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(auto mode:Modes)(void)Run(mode);
        SetNetworkManagerFactoryHostForAnalysis(nullptr);bool rejected=false;try{spNetworkManager m;}catch(const std::logic_error&){rejected=true;}Check(rejected,"missing host rejected");
        Check(spNetworkManager::StaticRTTI().classID==spNetworkManager::ClassID&&spNetworkManager::StaticRTTI().baseClassID==spBaseObject::ClassID,"RTTI contract");
        std::cout<<"spNetworkManager completed-operation checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
