#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::string Snapshot(const spDXRenderer::DrawStateForAnalysis& s)
    {
        std::ostringstream out;out<<'['<<s.boundVertex<<','<<s.boundPixel<<','<<s.vertex.top<<','<<s.pixel.top;
        for(const auto* stack:{&s.vertex,&s.pixel}){out<<",[";for(unsigned i=0;i<4;++i){if(i)out<<',';out<<stack->entries[i].identity;}out<<']';}out<<']';return out.str();
    }
    struct Sink
    {
        spDXRenderer::DrawStateForAnalysis state;std::vector<std::string> events;bool fail=false;
        static std::int32_t Shader(void* ptr,bool pixel,std::uintptr_t handle) noexcept
        {auto& s=*static_cast<Sink*>(ptr);std::ostringstream out;out<<"[\""<<(pixel?"pixel-shader":"vertex-shader")<<"\","<<handle<<','<<Snapshot(s.state)<<']';s.events.push_back(out.str());return s.fail?-1:0;}
        static std::int32_t Constants(void* ptr,bool pixel,const std::array<unsigned,4>* rows,std::size_t count) noexcept
        {
            auto& s=*static_cast<Sink*>(ptr);std::ostringstream out;out<<"[\""<<(pixel?"pixel-constants":"vertex-constants")<<"\",0,[";
            bool first=true;for(std::size_t i=0;i<count;++i)for(auto v:rows[i]){if(!first)out<<',';first=false;out<<v;}
            out<<"],"<<count<<','<<Snapshot(s.state)<<']';s.events.push_back(out.str());return s.fail?-1:0;
        }
        static std::int32_t Draw(void* ptr,bool indexed,const std::array<unsigned,6>& args) noexcept
        {auto& s=*static_cast<Sink*>(ptr);std::ostringstream out;out<<"[\""<<(indexed?"indexed":"draw")<<'"';for(unsigned i=0;i<(indexed?6u:3u);++i)out<<','<<args[i];out<<','<<Snapshot(s.state)<<']';s.events.push_back(out.str());return s.fail?-1:0;}
    };
    std::string Run(const std::string& mode)
    {
        const bool fixed=mode=="fixed"||mode=="fixed-failed"||mode=="fixed-pixel";
        Sink sink;sink.events.reserve(16);sink.fail=mode=="failed"||mode=="fixed-failed";auto& state=sink.state;
        spPCShaderManager manager;spDXMaterial material;material.SetSpecularPowerForAnalysis(0);
        std::array<spDXRenderer::ShaderBindingForAnalysis,4> shaders{{{1,101},{2,102},{3,103},{4,104}}};
        state.vertex.entries[0]=shaders[0];state.pixel.entries[0]=shaders[2];state.pixelEnabled=mode=="pixel"||mode=="constants"||mode=="failed";
        if(fixed)
        {
            state.vertex.entries[0]={};state.boundVertex=1;state.pixel.entries[0]={};
            state.pixelEnabled=mode=="fixed-pixel";if(state.pixelEnabled)state.pixel.entries[0]=shaders[0];
        }
        if(mode=="constants"||mode=="failed")
        {
            state.vertexConstants.resize(2);state.pixelConstants.resize(1);
            for(unsigned i=0;i<8;++i)state.vertexConstants[i/4][i%4]=0x3f000000+i;
            for(unsigned i=0;i<4;++i)state.pixelConstants[0][i]=0x40000000+i;
        }
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        if(mode=="stacks")
        {
            const std::array<std::pair<unsigned,unsigned>,6> actions{{{0x4be1b0,2},{0x4be1e0,4},{0x4be1b0,0},{0x4be1d0,0},{0x4be200,0},{0x4be1d0,0}}};
            for(auto [entry,ordinal]:actions)
            {
                const bool vertex=entry==0x4be1b0||entry==0x4be1d0;auto& stack=vertex?state.vertex:state.pixel;
                const bool pop=entry==0x4be1d0||entry==0x4be200;
                Check(pop?spDXRenderer::PopShaderForAnalysis(stack):spDXRenderer::PushShaderForAnalysis(stack,ordinal?shaders[ordinal-1]:spDXRenderer::ShaderBindingForAnalysis{}),"bounded shader stack");
                if(!first)out<<',';first=false;out<<'['<<entry<<','<<ordinal<<','<<Snapshot(state)<<']';
            }
        }
        else
        {
            unsigned step=0;for(unsigned kind:fixed?std::vector<unsigned>{0,1,2,3,4,2}:std::vector<unsigned>{0,1,2,3,4,2,2})
            {
                if(!fixed){if(step==3)state.vertex.entries[0]=shaders[1];if(step==5)state.pixel.entries[0]=shaders[3];if(step==6)state.pixel.entries[0]={};}
                const std::array<unsigned,7> args{kind,11,13,17,19,fixed?0x801u:0x2019u,23};sink.events.clear();
                const bool result=fixed?spDXRenderer::DrawWithoutBlendWeightsForAnalysis(state,args,manager,material,0,nullptr,{},Sink::Shader,Sink::Constants,Sink::Draw,&sink):
                    spDXRenderer::DrawPreselectedForAnalysis(state,args,Sink::Shader,Sink::Constants,Sink::Draw,&sink);Check(result,"original known draw branch");
                if(!first)out<<',';first=false;out<<'['<<step<<',';Array(out,args);out<<','<<result<<','<<Snapshot(state)<<",[";
                for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";++step;
            }
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"types","pixel","constants","failed","stacks","fixed","fixed-failed","fixed-pixel"})Run(mode);
            Sink sink;Check(!spDXRenderer::PopShaderForAnalysis(sink.state.vertex),"native stack underflow refused by host");
            sink.state.vertex.top=3;Check(!spDXRenderer::PushShaderForAnalysis(sink.state.vertex,{1,1}),"native stack overflow refused by host");
            Check(!spDXRenderer::DrawPreselectedForAnalysis(sink.state,{1,0,0,0,0,0,0},Sink::Shader,Sink::Constants,Sink::Draw,&sink)&&sink.events.empty(),"NULL vertex shader does not fake automatic generation");
            sink.state.vertex.top=0;sink.state.vertex.entries[0]={1,101};
            Check(!spDXRenderer::DrawPreselectedForAnalysis(sink.state,{5,0,0,0,0,0,0},Sink::Shader,Sink::Constants,Sink::Draw,&sink)&&sink.events.empty(),"native OOB primitive lookup host guard");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer preselected draw\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
