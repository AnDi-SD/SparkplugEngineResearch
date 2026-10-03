#include "Code/Sparkplug/spNetworkMatchmaking.h"
#include "Analysis/PC/spNetworkMatchmakingAbi.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    std::string Quote(const std::string& s)
    {
        std::string result="\"";
        for(char c:s){if(c=='"'||c=='\\')result+='\\';result+=c;}
        return result+'"';
    }
    struct Host final:spNetworkMatchmakingHost
    {
        std::vector<std::string> events;
        std::vector<std::uint8_t> payload{0,1,127,128,255};
        std::uint8_t connected=1;
        bool receive=true,close=true,debug=true,otherDebug=true,mutate=false;
        std::size_t live=0;
        Handle CreateSocketStream() override {events.push_back("create-socket");return ++live;}
        void DeleteSocketStream(Handle) noexcept override {events.push_back("delete-socket");--live;}
        std::uint8_t SocketConnected(Handle) override {events.push_back("connected:"+std::to_string(connected));return connected;}
        bool CloseSocketStream(Handle) override {events.push_back("close-socket");connected=0;return close;}
        bool ReceiveSocketStream(Handle,spMemoryStream& stream) override
        {
            Check(std::string(stream.GetStreamName())=="MM TCP packet buffer","receive diagnostic name");
            events.push_back("receive");
            if(!payload.empty())Check(stream.WriteData(payload.data(),static_cast<std::uint32_t>(payload.size())),"foreign receive writes stream");
            return receive;
        }
        Handle ResolveNetworkDebug() override {events.push_back("resolve-network");return debug?10:0;}
        Handle ResolveZeroReplyDebug() override {events.push_back("resolve-zero");return otherDebug?10:0;}
        void Log(Handle target,std::int32_t level,const std::string& category,const std::string& format,const std::string& argument) override
        {
            Check(category=="spNetworkMatchmaking","original log category");
            events.push_back("log:"+std::to_string(target)+":"+std::to_string(level)+":"+format+":"+argument);
        }
        void Dispatch(spNetworkMatchmaking& source,const spNetworkMatchmakingNotificationForAnalysis& n) override
        {
            Check(n.code==0x22&&n.sender==&source,"notification code/sender");
            Check(!n.word04&&!n.word08&&!n.word0C&&!n.word14,"notification zero fields");
            Check(n.size==payload.size()&&n.data==source.GetMemoryStreamForAnalysis()->GetBuffer(),"borrowed buffer and size");
            Check(std::equal(payload.begin(),payload.end(),static_cast<const std::uint8_t*>(n.data)),"exact received bytes");
            events.push_back("notify:"+std::to_string(n.code)+":"+std::to_string(n.size));
            if(mutate)
            {
                Check(source.GetMemoryStreamForAnalysis()->Seek(spStream::SeekSource::essStart,0),"subscriber rewinds");
                const std::uint8_t replacement=42;
                Check(source.GetMemoryStreamForAnalysis()->WriteData(&replacement,1),"subscriber mutates");
            }
        }
    };
    const char* Modes[]={"defaults","factory","clone","copy","copy-self","notify","close-ok","close-fail","close-no-debug","close-twice","pump-disconnected","pump-raw-connected","pump-failure","pump-partial-failure","pump-zero","pump-zero-no-debug","pump-zero-close-fail","pump-zero-other-debug-null","pump-bytes","pump-no-debug","pump-twice","pump-large","subscriber-mutate","lifetime-open"};
    std::string Snapshot(spNetworkMatchmaking& m)
    {
        const auto s=m.GetStateForAnalysis();auto* memory=m.GetMemoryStreamForAnalysis();
        std::ostringstream o;o<<'['<<int(s.hasSocketStream)<<','<<int(s.hasMemoryStream)<<','<<Quote(s.text)<<',';
        const auto name=memory->GetStreamName();o<<(name?Quote(name):"null")<<','<<int(memory->GetBuffer()!=nullptr)<<','<<memory->GetCapacity()<<']';return o.str();
    }
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known case");
        auto h=std::make_shared<Host>();SetNetworkMatchmakingFactoryHostForAnalysis(h);
        auto m=std::make_unique<spNetworkMatchmaking>();spCloneManager manager;
        Check(m->IsExactly(spNetworkMatchmaking::ClassID)&&m->IsKindOf(spBaseObject::ClassID),"RTTI queries");
        Check(!m->IsKindOf(spMemoryStream::ClassID),"unrelated RTTI");
        std::vector<std::string> states;std::vector<unsigned> results;
        auto state=[&](spNetworkMatchmaking& value){states.push_back(Snapshot(value));};
        if(mode=="factory"||mode=="clone"||mode=="copy")
        {
            if(mode!="factory")
            {
                auto* stream=m->GetMemoryStreamForAnalysis();Check(stream->Open("source"),"source open");
                Check(stream->WriteData(h->payload.data(),static_cast<std::uint32_t>(h->payload.size())),"source bytes");
            }
            auto second=mode=="clone"?m->vfunc_10(manager):mode=="factory"?spRTTIManager::Instance().Create(spNetworkMatchmaking::ClassID):std::make_unique<spNetworkMatchmaking>();
            Check(bool(second),"factory/clone object");
            results.push_back(mode=="clone"?manager.FindClone(*m)==second.get():mode=="copy"?m->vfunc_14(*second,manager):1);
            state(*m);state(static_cast<spNetworkMatchmaking&>(*second));second.reset();
        }
        else if(mode=="copy-self") {results.push_back(m->vfunc_14(*m,manager));state(*m);}
        else if(mode=="notify") {std::uint32_t code=0x22;m->vfunc_0C(&code);state(*m);}
        else if(mode.rfind("close-",0)==0)
        {
            h->close=mode=="close-ok"||mode=="close-twice";h->debug=mode!="close-no-debug";
            results.push_back(m->CloseForAnalysis());if(mode=="close-twice")results.push_back(m->CloseForAnalysis());state(*m);
        }
        else if(mode=="defaults")state(*m);
        else if(mode=="lifetime-open"){Check(m->GetMemoryStreamForAnalysis()->Open("retained"),"owned stream open");state(*m);}
        else
        {
            if(mode=="pump-disconnected")h->connected=0;
            if(mode=="pump-raw-connected")h->connected=255;
            if(mode=="pump-failure"||mode=="pump-partial-failure")h->receive=false;
            if(mode=="pump-failure"||mode.rfind("pump-zero",0)==0)h->payload.clear();
            if(mode=="pump-zero-close-fail")h->close=false;
            if(mode=="pump-zero-no-debug"||mode=="pump-no-debug")h->debug=false;
            if(mode=="pump-zero-other-debug-null")h->otherDebug=false;
            if(mode=="pump-large"){h->payload.resize(256);for(unsigned i=0;i<256;++i)h->payload[i]=std::uint8_t(i);}
            h->mutate=mode=="subscriber-mutate";
            m->PumpForAnalysis();state(*m);
            if(mode=="pump-twice"){m->PumpForAnalysis();state(*m);}
            Check(!m->GetMemoryStreamForAnalysis()->GetBuffer(),"completed pump closes buffer");
        }
        m.reset();Check(!h->live,"all sockets released");
        Check(h->events.back()=="delete-socket","destructor does not run own Close");
        SetNetworkMatchmakingFactoryHostForAnalysis(nullptr);
        std::ostringstream o;o<<"{\"events\":[";
        for(std::size_t i=0;i<h->events.size();++i){if(i)o<<',';o<<Quote(h->events[i]);}
        o<<"],\"results\":[";for(std::size_t i=0;i<results.size();++i){if(i)o<<',';o<<results[i];}
        o<<"],\"states\":[";for(std::size_t i=0;i<states.size();++i){if(i)o<<',';o<<states[i];}o<<"]}";return o.str();
    }
    void HostLifetime()
    {
        auto first=std::make_shared<Host>();auto second=std::make_shared<Host>();
        SetNetworkMatchmakingFactoryHostForAnalysis(first);auto source=std::make_unique<spNetworkMatchmaking>();
        SetNetworkMatchmakingFactoryHostForAnalysis(second);spCloneManager manager;auto clone=source->vfunc_10(manager);
        Check(first->live==1&&second->live==1,"clone uses current factory host");
        source.reset();Check(!first->live&&second->live==1,"old source retains its host");clone.reset();
        SetNetworkMatchmakingFactoryHostForAnalysis(nullptr);
        bool rejected=false;try{spNetworkMatchmaking missing;}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"missing factory host is explicit");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:Modes)(void)Run(mode);HostLifetime();
        std::cout<<"PASS NetworkMatchmaking completed operations and host lifetime\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
