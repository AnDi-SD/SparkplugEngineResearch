#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;int checks=0;
    void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
    std::string Hex(const void* data,std::size_t n)
    {const auto* p=static_cast<const unsigned char*>(data);constexpr char h[]="0123456789abcdef";std::string out;for(std::size_t i=0;i<n;++i){out+=h[p[i]>>4];out+=h[p[i]&15];}return out;}
    std::array<std::string,3> Inputs(const std::string& mode)
    {
        unsigned kind=mode=="disabled"?0:mode=="exp"?1:mode=="exp2"||mode=="raw-density"?2:mode=="unknown"?4:3;
        std::array<std::array<unsigned,5>,3> words{{{kind,0x12345678,0xc0000000,0x42f60000,0x3e800000},
            {1,0x89abcdef,0,0x3f800000,0x3f000000},{0,0xff000000,0,0x3f800000,0x3f800000}}};
        if(mode=="raw-linear")words[0]={3,0x12345678,0x80000000,0x7fc12345,0x7f800000};
        if(mode=="raw-density")words[0]={2,0x12345678,0,0x3f800000,0x7fc12345};
        std::array<std::string,3> result;for(unsigned i=0;i<3;++i)result[i]="a014"+Hex(words[i].data(),20)+"00";return result;
    }
    struct Sink
    {
        R::FogStateForAnalysis state;std::array<unsigned,256> cache{};std::array<spFog,3> objects;
        std::vector<std::array<unsigned,4>> events;bool fail=false;
        unsigned Token()const {if(!state.current)return 0;for(unsigned i=0;i<3;++i)if(state.current==&objects[i])return i+1;return 99;}
        static std::int32_t Submit(void* p,unsigned index,unsigned value)noexcept
        {auto& s=*static_cast<Sink*>(p);s.events.push_back({index,value,s.Token(),s.cache[index]});return s.fail?-1:0;}
    };
    std::string Run(const std::string& mode,const std::array<std::string,3>& wire)
    {
        Sink s;s.fail=mode=="failed-device";s.cache.fill(0xa5a5a5a5);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        for(unsigned i=0;i<3;++i)
        {
            spMemoryStream stream;Check(wire[i].size()==46&&stream.ResizeAndSetSize(23),"bounded Fog field stream");
            auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());for(unsigned j=0;j<23;++j)bytes[j]=static_cast<unsigned char>(std::stoul(wire[i].substr(2*j,2),nullptr,16));
            std::string error;Check(spFogSerializer{}.ReadPayloadForAnalysis(context,stream,23,s.objects[i],&error),error.c_str());
            unsigned cursor=0;Check(stream.GetCurrentPosition(cursor)&&cursor==23,"complete Fog read");
        }
        s.state.fallback=&s.objects[2];std::vector<const spFog*> inputs{&s.objects[0],&s.objects[0]};
        if(mode=="cache")inputs.insert(inputs.end(),{&s.objects[0],&s.objects[1],nullptr,nullptr,&s.objects[0]});
        else inputs.insert(inputs.end(),{nullptr,nullptr});
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned i=0;i<3;++i){if(i)out<<',';out<<'"'<<wire[i]<<'"';}out<<"],[";
        for(std::size_t i=0;i<inputs.size();++i)
        {
            if(mode=="cache"&&i==2){s.objects[0].SetColorARGBForAnalysis(0xa1b2c3d4);unsigned word=0x80000000;float value;std::memcpy(&value,&word,4);s.objects[0].SetStartForAnalysis(value);}
            s.events.clear();const auto result=R::ApplyFogForAnalysis(s.state,inputs[i],s.cache,Sink::Submit,&s);
            Check(result==(mode!="unknown"||i!=0),"native invalid-type identity cache result");
            if(i)out<<',';out<<'['<<result<<','<<s.Token()<<",[";bool first=true;
            for(unsigned index:{0x1cu,0x22u,0x23u,0x24u,0x25u,0x26u}){if(!first)out<<',';first=false;out<<s.cache[index];}out<<"],[";
            for(std::size_t j=0;j<s.events.size();++j){if(j)out<<',';out<<'[';for(unsigned k=0;k<4;++k){if(k)out<<',';out<<s.events[j][k];}out<<']';}out<<"]]";
        }
        out<<"]]";return out.str();
    }
    void Guards()
    {
        Sink s;Check(R::ApplyFogForAnalysis(s.state,nullptr,s.cache,nullptr,nullptr),"unchanged NULL has no dereference or callback");
        s.state.current=&s.objects[0];Check(!R::ApplyFogForAnalysis(s.state,nullptr,s.cache,Sink::Submit,&s)&&s.state.current==&s.objects[0],"missing fallback guard avoids native NULL dereference");
        s.objects[0].SetTypeForAnalysis(spFog::Type::Linear);s.state.current=nullptr;
        Check(!R::ApplyFogForAnalysis(s.state,&s.objects[0],s.cache,nullptr,nullptr)&&s.state.current==&s.objects[0],"missing device guard preserves earlier identity publication");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")
        {std::array<std::string,3> wire;for(auto& value:wire)std::cin>>value;std::cout<<Run(argv[2],wire)<<'\n';return 0;}
        for(const char* mode:{"disabled","exp","exp2","linear","unknown","raw-linear","raw-density","failed-device","cache"})(void)Run(mode,Inputs(mode));
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": decoded Fog renderer identity and raw state cache\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
