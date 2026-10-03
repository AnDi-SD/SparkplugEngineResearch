#include "Code/Sparkplug/spNetworkDebug.h"
#include "Analysis/PC/spNetworkDebugAbi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    std::string Quote(const std::string& value)
    {
        std::string out="\"";
        for(unsigned char c:value)
        {
            if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
            else if(c=='\r')out+="\\r";else if(c=='\n')out+="\\n";else if(c=='\t')out+="\\t";
            else if(c<32){char b[7];std::snprintf(b,sizeof(b),"\\u%04x",c);out+=b;}else out+=char(c);
        }
        return out+'"';
    }
    struct Host final:spNetworkDebugHost
    {
        std::vector<std::string> events;
        std::uint32_t now=1200,raw=1234,divisor=1,bias=10;
        spNetworkDebug* debug=nullptr;
        int lockDepth=0,streams=0,locks=0;
        bool failOpen=false;
        bool failFormat=false,failWrite=false;
        struct Stream final:spNetworkDebugStreamForAnalysis
        {
            Host& host;
            explicit Stream(Host& h):host(h){++host.streams;}
            ~Stream() override{host.events.push_back("delete-stream");--host.streams;}
            bool Open(const std::string& path,std::uint32_t mode) override{host.events.push_back("open:"+path+":"+std::to_string(mode));return !host.failOpen;}
            bool Close() noexcept override{host.events.push_back("close");return false;}
            void Seek(std::uint32_t origin,std::uint32_t position) override{host.events.push_back("seek:"+std::to_string(origin)+":"+std::to_string(position));}
            void WriteLine(const std::string& text,std::uint32_t argument) override{Check(argument==0,"native WriteLine argument");if(host.failWrite)throw std::runtime_error("supplied host write failure");host.events.push_back("file:"+text);}
        };
        std::uint32_t TimeGetTime() noexcept override{events.push_back("clock:"+std::to_string(now));return now;}
        std::uintptr_t CreateLock() override{events.push_back("create-lock");++locks;return 1;}
        void FreeLockStorage(std::uintptr_t) noexcept override{events.push_back("free-lock");--locks;}
        void EnterLock(std::uintptr_t) override{events.push_back("enter");++lockDepth;}
        void LeaveLock(std::uintptr_t) noexcept override{events.push_back("leave");--lockDepth;}
        std::string LogDirectory() override{return "logs/";}
        std::unique_ptr<spNetworkDebugStreamForAnalysis> CreateStream() override{events.push_back("create-stream");return std::make_unique<Stream>(*this);}
        EngineClock ReadEngineClock() override{events.push_back("engine-clock");return {raw,divisor,bias};}
        std::uint32_t TimerDivisor() override{return divisor;}
        std::string FormatMessage(const char* format,std::va_list arguments,std::uint32_t capacity) override
        {
            Check(lockDepth==1&&capacity==8191,"original formatting runs under lock");
            if(failFormat)throw std::runtime_error("supplied host format failure");
            char buffer[8192];int size=std::vsnprintf(buffer,sizeof(buffer),format,arguments);
            Check(size>=0&&size<8191,"bounded imported CRT specimen");return buffer;
        }
        std::string FormatFrameRate(float value) override
        {
            std::uint32_t bits;std::memcpy(&bits,&value,4);events.push_back("rate:"+std::to_string(bits));return "frame:"+std::to_string(bits);
        }
        void ConsoleOutput(const std::string& text) override{events.push_back("console:"+text);}
        std::uintptr_t ReadContextField14(std::uintptr_t) override{return 256;}
        void DrawFrameRate(std::uint32_t x,std::uintptr_t target,const std::string& text,std::uint32_t arg) override
        {events.push_back("draw:"+std::to_string(x)+":"+std::to_string(target)+":"+text+":"+std::to_string(arg));}
        void Unsubscribe(spNetworkDebug&) override{events.push_back("unsubscribe");}
        spNetworkDebug* PacketDebug() override{return debug;}
    };
    std::string Snapshot(const spNetworkDebug& debug)
    {
        const auto s=debug.GetStateForAnalysis();std::ostringstream o;
        o<<'['<<unsigned(s.consoleEnabled)<<','<<unsigned(s.fileEnabled)<<','<<unsigned(s.unknown12)<<','<<unsigned(s.subscribed)<<','
            <<int(s.hasStream)<<','<<int(s.hasLock)<<','<<Quote(s.lastLine)<<','<<unsigned(s.timerActive)<<','<<s.accumulated<<','<<s.start<<','<<int(s.context!=0)<<','<<s.level<<']';return o.str();
    }
    const char* Modes[]={"defaults","start","stop","stop-twice","restart","timer-wrap","file-on","file-off","file-reenable","file-failure","file-repeat-off","stop-file","stop-disabled-file","unsubscribe","copy","copy-self","clone","factory","notify-other","render","render-zero","render-large","render-divisor","render-stopped","notify-render","log-disabled","log-filter","log-console","log-file","log-both","log-negative","log-signed-clock","log-wrapped-clock","log-long","dump-level","dump-empty","dump-bytes","dump-null","dump-filter"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"unknown case");
        auto h=std::make_shared<Host>();SetNetworkDebugFactoryHostForAnalysis(h);
        auto source=std::make_unique<spNetworkDebug>();h->debug=source.get();
        std::vector<std::string> states;std::vector<std::uint32_t> results;
        auto state=[&](const spNetworkDebug& d){states.push_back(Snapshot(d));};
        auto controls=[&](std::uint8_t console,std::int32_t level=1,std::uint8_t subscription=0)
        {source->SetControlsForAnalysis(console,127,subscription,1,level);};
        spCloneManager manager;
        if(mode=="defaults")state(*source);
        else if(mode=="start"){h->now=1300;results.push_back(source->StartForAnalysis());state(*source);}
        else if(mode=="stop"||mode=="stop-twice"||mode=="restart"||mode=="timer-wrap")
        {
            h->now=mode=="timer-wrap"?0:1500;source->StopForAnalysis();state(*source);
            if(mode=="stop-twice"){h->now=1600;source->StopForAnalysis();state(*source);}
            if(mode=="restart"){h->now=2000;results.push_back(source->StartForAnalysis());state(*source);}
        }
        else if(mode.rfind("file-",0)==0||mode=="stop-file"||mode=="stop-disabled-file")
        {
            h->failOpen=mode=="file-failure";source->SetFileOutputForAnalysis(255,"net.log");state(*source);
            if(mode=="file-off"||mode=="file-reenable"||mode=="file-repeat-off"||mode=="stop-disabled-file")
            {source->SetFileOutputForAnalysis(0,"unused");state(*source);}
            if(mode=="file-reenable"){source->SetFileOutputForAnalysis(1,"ignored.log");state(*source);}
            if(mode=="file-repeat-off"){source->SetFileOutputForAnalysis(0,"unused");state(*source);}
            if(mode=="stop-file"||mode=="stop-disabled-file"){source->StopForAnalysis();state(*source);}
        }
        else if(mode=="unsubscribe"){controls(255,1,127);source->StopForAnalysis();state(*source);}
        else if(mode=="copy"||mode=="copy-self"||mode=="clone"||mode=="factory")
        {
            controls(255,7);source->SetFileOutputForAnalysis(1,"copy.log");source->LogForAnalysis("cat",1,"%s %d","item",-5);
            if(mode=="copy-self"){results.push_back(source->vfunc_14(*source,manager));state(*source);}
            else
            {
                std::unique_ptr<spBaseObject> object=mode=="clone"?source->vfunc_10(manager):spRTTIManager::Instance().Create(spNetworkDebug::StaticRTTI().classID);
                auto* d=dynamic_cast<spNetworkDebug*>(object.get());Check(d!=nullptr,"RTTI creates debug");
                if(mode=="copy")results.push_back(source->vfunc_14(*d,manager));
                if(mode=="clone")results.push_back(manager.FindClone(*source)==d);
                state(*source);state(*d);object.reset();
            }
        }
        else if(mode=="notify-other"){std::uint32_t code=25;source->vfunc_0C(&code);state(*source);}
        else if(mode.rfind("render",0)==0||mode=="notify-render")
        {
            controls(0);h->now=mode=="render-zero"?1200:mode=="render-large"?0xffffffff:1450;
            if(mode=="render-divisor")h->divisor=3;
            if(mode=="render-stopped"){source->StopForAnalysis();h->now=9000;}
            if(mode=="notify-render"){std::uint32_t code=26;source->vfunc_0C(&code);}else source->RenderFrameRateForAnalysis();state(*source);
        }
        else if(mode.rfind("log-",0)==0)
        {
            const bool file=mode=="log-file"||mode=="log-both";
            controls(mode=="log-disabled"||mode=="log-file"?0:255,mode=="log-negative"?-2:1);
            if(file)source->SetFileOutputForAnalysis(1,"net.log");
            if(mode=="log-signed-clock")h->raw=0x80000000;
            if(mode=="log-wrapped-clock"){h->raw=0xffffffff;h->bias=100;}
            if(mode=="log-long")source->LogForAnalysis("cat",1,"%s",std::string(100,'x').c_str());
            else source->LogForAnalysis("cat",mode=="log-filter"?2:mode=="log-negative"?-3:1,"%s %d","item",-5);
            state(*source);
        }
        else if(mode.rfind("dump-",0)==0)
        {
            controls(255,mode=="dump-level"?1:2);
            if(mode=="dump-null")h->debug=nullptr;
            std::unique_ptr<spNetworkDebug> alternate;
            if(mode=="dump-filter"){alternate=std::make_unique<spNetworkDebug>();h->debug=alternate.get();alternate->SetControlsForAnalysis(255,0,0,0,1);}
            spNetworkDebug::PacketForAnalysis packet{0xbad0,0x1234,65535,{}};
            if(mode=="dump-bytes")packet.data={0,1,15,16,127,128,254,255};
            source->DumpPacketForAnalysis(packet);state(*source);alternate.reset();
        }
        source.reset();SetNetworkDebugFactoryHostForAnalysis(nullptr);
        Check(h->locks==0&&h->streams==0&&h->lockDepth==0,"owned lifetime and output lock balance");
        std::ostringstream o;o<<"{\"results\":[";
        for(std::size_t i=0;i<results.size();++i){if(i)o<<',';o<<results[i];}o<<"],\"states\":[";
        for(std::size_t i=0;i<states.size();++i){if(i)o<<',';o<<states[i];}o<<"],\"events\":[";
        for(std::size_t i=0;i<h->events.size();++i){if(i)o<<',';o<<Quote(h->events[i]);}return o.str()+"]}";
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const char* mode:Modes)(void)Run(mode);
        // Additional portable cleanup checks, not native behavior claims.
        for(int failure=0;failure<3;++failure)
        {
            auto h=std::make_shared<Host>();
            {
                spNetworkDebug debug(h);debug.SetControlsForAnalysis(1,0,0,0,1);
                h->failFormat=failure==0;h->divisor=failure==1?0:1;
                if(failure==2){debug.SetFileOutputForAnalysis(1,"net.log");h->failWrite=true;}
                bool threw=false;
                try{debug.LogForAnalysis("guard",1,"%d",5);}catch(const std::runtime_error&){threw=true;}catch(const std::logic_error&){threw=true;}
                Check(threw&&h->lockDepth==0,"portable host failure releases entered output lock");
            }
            Check(h->locks==0&&h->streams==0,"failed portable output retains balanced owner lifetime");
        }
        SetNetworkDebugFactoryHostForAnalysis(nullptr);bool threw=false;
        try{spNetworkDebug missing;}catch(const std::logic_error&){threw=true;}Check(threw,"missing host fails explicitly");
        std::cout<<"PASS "<<std::size(Modes)<<" NetworkDebug cases and 4 portable host guards\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
