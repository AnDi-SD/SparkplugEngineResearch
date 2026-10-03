#include "Code/Sparkplug/spNetworkStateCtrl.h"
#include "Analysis/PC/spNetworkStateCtrlAbi.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    struct Host final:spNetworkStateCtrlHost
    {
        std::vector<std::string> events;
        std::map<Handle,unsigned> depth;
        Handle next=0,currentManager=10;
        bool startResult=true,switchManager=false,reenter=false;
        spNetworkStateCtrl* source=nullptr;
        Handle CreateLock() override{const auto h=++next;depth[h]=0;events.push_back("create-lock:"+std::to_string(h));return h;}
        void FreeLockStorage(Handle h) noexcept override{events.push_back("free-lock:"+std::to_string(h));depth.erase(h);}
        void EnterLock(Handle h) override{Check(depth.count(h)!=0,"enter live lock");++depth[h];events.push_back("enter:"+std::to_string(h));}
        void LeaveLock(Handle h) noexcept override{events.push_back("leave:"+std::to_string(h));--depth[h];}
        Handle ResolveNetworkManager() override{events.push_back("resolve:"+std::to_string(currentManager));return currentManager;}
        bool StartConnection(Handle manager) override
        {
            events.push_back("start:"+std::to_string(manager));
            if(reenter){reenter=false;source->RequestForAnalysis(6,1);}
            return startResult;
        }
        void ManagerOperation450B70(Handle manager) override{events.push_back("op450B70:"+std::to_string(manager));if(switchManager)currentManager=20;}
        void ManagerOperation4516F0(Handle manager,std::uint32_t value) override{events.push_back("op4516F0:"+std::to_string(manager)+":"+std::to_string(value));}
        void RefreshTimer(Handle manager) override{events.push_back("refresh:"+std::to_string(manager));}
    };
    const char* Modes[]={"defaults","factory","clone","copy","copy-self","notify","pump-empty","immediate-sequence","queued-sequence","queue-pending","request-raw-immediate","immediate-action3","queued-action3","immediate-action4","queued-action4","raw-changed","stale-result","entry1-false","entry2-switch-manager","reentrant-immediate","reentrant-pump","matrix"};
    std::string Snapshot(spNetworkStateCtrl& value)
    {
        const auto s=value.GetStateForAnalysis();std::ostringstream o;
        o<<'['<<unsigned(s.changed)<<','<<s.target<<','<<s.previous<<','<<s.current<<','<<s.result<<",[";
        for(std::size_t i=0;i<s.queued.size();++i){if(i)o<<',';o<<s.queued[i];}return o.str()+"]]";
    }
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known case");
        auto h=std::make_shared<Host>();SetNetworkStateCtrlFactoryHostForAnalysis(h);
        auto source=std::make_unique<spNetworkStateCtrl>();h->source=source.get();spCloneManager manager;
        Check(source->IsExactly(spNetworkStateCtrl::ClassID)&&source->IsKindOf(spBaseObject::ClassID),"RTTI and physical root");
        std::vector<std::string> states;std::vector<std::uint32_t> results;
        auto state=[&](spNetworkStateCtrl& value){states.push_back(Snapshot(value));};
        auto apply=[&](std::uint32_t event){results.push_back(source->ApplyImmediateForAnalysis(event));state(*source);};
        if(mode=="factory"||mode=="clone"||mode=="copy")
        {
            source->SetStateForAnalysis(255,3,2,1,4);source->RequestForAnalysis(8,0);
            auto second=mode=="clone"?source->vfunc_10(manager):mode=="factory"?spRTTIManager::Instance().Create(spNetworkStateCtrl::ClassID):std::make_unique<spNetworkStateCtrl>();
            Check(bool(second),"created second object");
            results.push_back(mode=="clone"?manager.FindClone(*source)==second.get():mode=="copy"?source->vfunc_14(*second,manager):1);
            state(*source);state(static_cast<spNetworkStateCtrl&>(*second));second.reset();
        }
        else if(mode=="copy-self"){results.push_back(source->vfunc_14(*source,manager));state(*source);}
        else if(mode=="notify"){std::uint32_t code=34;source->vfunc_0C(&code);state(*source);}
        else if(mode=="defaults")state(*source);
        else if(mode=="pump-empty"){source->SetStateForAnalysis(255,2,1,3,4);source->PumpForAnalysis();state(*source);}
        else if(mode=="immediate-sequence")for(auto event:{4u,9u,0u,5u,9u,0u,7u,9u,0u,8u,9u,4u})apply(event);
        else if(mode=="queued-sequence"||mode=="queue-pending")
        {
            for(auto event:{4u,9u,5u,7u,8u}){source->RequestForAnalysis(event,0);state(*source);}
            if(mode=="queued-sequence")for(unsigned i=0;i<6;++i){source->PumpForAnalysis();state(*source);}
        }
        else if(mode=="request-raw-immediate"){source->RequestForAnalysis(4,255);state(*source);}
        else if(mode=="immediate-action3"||mode=="queued-action3"||mode=="immediate-action4"||mode=="queued-action4")
        {
            source->SetStateForAnalysis(0,0xffffffffu,0xffffffffu,1,0);const auto event=mode.back()=='3'?6u:8u;
            if(mode.rfind("immediate-",0)==0)apply(event);
            else{source->RequestForAnalysis(event,0);source->PumpForAnalysis();state(*source);}
        }
        else if(mode=="raw-changed"){source->SetStateForAnalysis(255,2,123,4,0);apply(9);}
        else if(mode=="stale-result"){source->SetStateForAnalysis(255,2,123,0,7);apply(9);apply(4);}
        else if(mode=="entry1-false"){h->startResult=false;source->SetStateForAnalysis(0,0xffffffffu,0xffffffffu,1,0);apply(0);}
        else if(mode=="entry2-switch-manager"){h->switchManager=true;source->SetStateForAnalysis(0,0xffffffffu,0xffffffffu,2,0);apply(0);}
        else if(mode=="reentrant-immediate"||mode=="reentrant-pump")
        {
            h->reenter=true;
            if(mode=="reentrant-immediate"){source->RequestForAnalysis(4,1);state(*source);}
            else{source->RequestForAnalysis(4,0);source->PumpForAnalysis();state(*source);}
        }
        else if(mode=="matrix")
        {
            for(auto current:{0u,1u,2u,3u,4u,0xffffffffu})for(auto event:{0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,0xffffffffu})
            {source->SetStateForAnalysis(0,0xffffffffu,0xffffffffu,current,0);apply(event);}
        }
        source.reset();Check(h->depth.empty(),"all owned lock storage freed");SetNetworkStateCtrlFactoryHostForAnalysis(nullptr);
        std::ostringstream o;o<<"{\"events\":[";
        for(std::size_t i=0;i<h->events.size();++i){if(i)o<<',';o<<'"'<<h->events[i]<<'"';}
        o<<"],\"results\":[";for(std::size_t i=0;i<results.size();++i){if(i)o<<',';o<<results[i];}
        o<<"],\"states\":[";for(std::size_t i=0;i<states.size();++i){if(i)o<<',';o<<states[i];}return o.str()+"]}";
    }
    void HostLifetime()
    {
        auto first=std::make_shared<Host>();auto second=std::make_shared<Host>();
        SetNetworkStateCtrlFactoryHostForAnalysis(first);auto source=std::make_unique<spNetworkStateCtrl>();
        SetNetworkStateCtrlFactoryHostForAnalysis(second);spCloneManager manager;auto clone=source->vfunc_10(manager);
        Check(first->depth.size()==1&&second->depth.size()==1,"clone uses current factory host");source.reset();
        Check(first->depth.empty()&&second->depth.size()==1,"existing object retains original host");clone.reset();
        SetNetworkStateCtrlFactoryHostForAnalysis(nullptr);bool rejected=false;
        try{spNetworkStateCtrl missing;}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"missing foreign host rejected");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:Modes)(void)Run(mode);HostLifetime();
        std::cout<<"PASS NetworkStateCtrl completed operations, reentrancy and host lifetime\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
