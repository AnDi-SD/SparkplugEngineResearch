#include "Code/SparkplugDX/spDXRenderer.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    struct Sink
    {
        R::LightSubmissionStateForAnalysis state;bool fail=false;std::vector<std::string> events;
        std::string State()const
        {std::ostringstream o;o<<'['<<state.previousCount<<','<<state.borrowedList<<',';Array(o,state.grouped);o<<',';std::array<std::uint32_t,4> raw{};for(unsigned i=0;i<4;++i)raw[i]=Bits(state.ambient[i]);Array(o,raw);o<<','<<state.deviceAmbient<<']';return o.str();}
        std::int32_t Event(const std::string& content){events.push_back('['+content+','+State()+']');return fail?-1:0;}
        static std::int32_t Light(void* ptr,unsigned index,const std::array<std::uint32_t,26>& words)noexcept
        {std::ostringstream o;o<<"\"light\","<<index<<',';Array(o,words);return static_cast<Sink*>(ptr)->Event(o.str());}
        static std::int32_t Enable(void* ptr,unsigned index,bool value)noexcept
        {return static_cast<Sink*>(ptr)->Event("\"enable\","+std::to_string(index)+','+std::to_string(value));}
        static std::int32_t Render(void* ptr,unsigned index,unsigned value)noexcept
        {return static_cast<Sink*>(ptr)->Event("\"render\","+std::to_string(index)+','+std::to_string(value));}
    };
    std::string Run(const std::string& mode)
    {
        Sink sink;sink.fail=mode=="failed";sink.state.previousCount=5;sink.state.borrowedList=9;
        std::array<R::DeviceLightInputForAnalysis,3> lights{};R::LightListInputForAnalysis list;
        for(unsigned i=0;i<3;++i)
        {auto& light=lights[i];light.identity=i+1;light.type=std::array<unsigned,3>{2,0,1}[i];light.enabled=i!=1;light.color={.25F,.5F,.75F,1.F};for(unsigned j=0;j<26;++j)light.deviceWords[j]=0x10000000+i*256+j;}
        if(mode=="ambient"||mode=="tolerance")list.ambient=&lights[0];
        if(mode=="null"){for(unsigned i=0;i<24;++i)sink.state.grouped[i]=i%3+1;sink.state.ambient.fill(.75F);}
        std::uint32_t fallback=0x7f234567;std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned iteration=0;iteration<4;++iteration)
        {
            list.lights.clear();for(unsigned i=0;i<std::array<unsigned,4>{3,3,1,0}[iteration];++i)list.lights.push_back(&lights[i]);
            if(mode=="fallback"&&iteration==2)fallback=0xff123456;
            if(mode=="ambient"&&iteration==2)lights[0].enabled=false;
            if(mode=="tolerance")lights[0].color[0]=std::array<float,4>{.25F,.2505F,.2511F,.2511F}[iteration];
            sink.events.clear();const bool result=R::SubmitLightsForAnalysis(sink.state,mode=="null"?nullptr:&list,fallback,Sink::Light,Sink::Enable,Sink::Render,&sink);Check(result,"native light submission");
            if(iteration)out<<',';out<<'['<<iteration<<','<<result<<','<<sink.State()<<",[";
            for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"list","failed","ambient","fallback","tolerance","null"})(void)Run(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer light submission\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
