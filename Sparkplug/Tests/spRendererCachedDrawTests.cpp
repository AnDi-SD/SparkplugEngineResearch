#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    template<class T>void Array(std::ostream& o,const T& values){o<<'[';bool first=true;for(const auto& v:values){if(!first)o<<',';first=false;o<<v;}o<<']';}
    struct Sink
    {
        R::DrawStateForAnalysis state;bool fail=false;std::uintptr_t identity=0;std::vector<std::string> events;
        std::string State()const
        {std::ostringstream o;std::vector<unsigned> raw;for(const auto& row:state.vertexConstants)for(auto word:row)raw.push_back(word);o<<'['<<(state.vertex.entries[0].identity==identity)<<','<<(state.boundVertex==identity)<<','<<state.vertexConstants.size()<<',';Array(o,raw);o<<']';return o.str();}
        std::int32_t Event(const std::string& content){events.push_back('['+content+','+State()+']');return fail?-1:0;}
        static std::int32_t Shader(void* ptr,bool pixel,std::uintptr_t value)noexcept
        {return static_cast<Sink*>(ptr)->Event(std::string(pixel?"\"pixel-shader\",":"\"vertex-shader\",")+std::to_string(value));}
        static std::int32_t Constants(void* ptr,bool pixel,const std::array<unsigned,4>* data,std::size_t rows)noexcept
        {std::ostringstream o;std::vector<unsigned> words;for(unsigned i=0;i<rows;++i)for(auto word:data[i])words.push_back(word);o<<(pixel?"\"pixel-constants\",0,":"\"vertex-constants\",0,");Array(o,words);o<<','<<rows;return static_cast<Sink*>(ptr)->Event(o.str());}
        static std::int32_t Draw(void* ptr,bool indexed,const std::array<unsigned,6>& args)noexcept
        {std::ostringstream o;o<<(indexed?"\"indexed\"":"\"draw\"");for(unsigned i=0;i<(indexed?6u:3u);++i)o<<','<<args[i];return static_cast<Sink*>(ptr)->Event(o.str());}
    };
    std::string Run(const std::string& mode)
    {
        Sink sink;sink.fail=mode=="failed";spPCShaderManager manager;spDXMaterial material;material.SetSpecularPowerForAnalysis(0);
        auto shader=std::make_unique<spPCVertexShader>();sink.identity=reinterpret_cast<std::uintptr_t>(shader.get());
        spDXShader::ScalarWordsForAnalysis scalar{};scalar[8]=4;shader->SetScalarWordsForAnalysis(scalar);
        shader->SetParametersForAnalysis({{{},5,0,3},{{},8,3,1}});Check(manager.CacheShaderForAnalysis({0x11,0},shader),"actual cached key");
        spDXShader::ConstantInputsForAnalysis inputs;inputs.material=&material;inputs.blendMatrices.resize(1);
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned iteration=0;iteration<4;++iteration)
        {
            for(unsigned i=0;i<16;++i)inputs.blendMatrices[0][i]=Bits(float(iteration*100+i));
            material.SetDiffuseColorForAnalysis({.25F,.5F,.75F,float(iteration)});
            if(mode=="preselected"&&iteration==1)sink.state.vertex.entries[0]={sink.identity,0};
            sink.events.clear();const bool result=R::DrawCachedAutomaticForAnalysis(sink.state,{iteration==3?1u:2u,11,13,17,19,0x803,0},manager,inputs,0,nullptr,{},Sink::Shader,Sink::Constants,Sink::Draw,&sink);Check(result,"cached weighted automatic draw");
            if(iteration)out<<',';out<<'['<<iteration<<','<<result<<','<<sink.State()<<",[";
            for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"cached","failed","preselected"})(void)Run(mode);
        spPCVertexShader shader;spDXShader::ConstantInputsForAnalysis input;std::vector<unsigned> output;
        shader.SetParametersForAnalysis({{{},17,0,3}});input.uvMatrices.resize(1);Check(!shader.BuildFullyWrittenConstantsForAnalysis(output,input,3),"UV fourth lanes are unknown, not zero");
        shader.SetParametersForAnalysis({});Check(!shader.BuildFullyWrittenConstantsForAnalysis(output,input,1),"empty descriptor cannot populate row");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": cached automatic shader draw\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
