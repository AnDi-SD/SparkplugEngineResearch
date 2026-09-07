#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;unsigned checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<value;}out<<']';}
    void Quoted(std::ostream& out,std::string_view value)
    {out<<'"';for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32)out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];else out<<c;}out<<'"';}
    struct Sink
    {
        R::DrawStateForAnalysis state;bool fail=false,valid=true;std::vector<std::string> events;
        std::string State() const
        {std::ostringstream out;std::array<unsigned,4> words{};if(!state.vertexConstants.empty())words=state.vertexConstants[0];out<<'['<<unsigned(state.vertex.entries[0].identity!=0)<<','<<unsigned(state.boundVertex!=0)<<','<<state.vertexConstants.size()<<',';Array(out,words);out<<']';return out.str();}
        std::int32_t Event(std::string text){events.push_back('['+text+','+State()+']');return fail?-1:0;}
        static std::int32_t Create(void* raw,const unsigned* code,std::uintptr_t* output) noexcept
        {
            auto& sink=*static_cast<Sink*>(raw);std::string hex;
            if(!code)sink.valid=false;
            else for(unsigned i=0;i<5;++i){auto byte=reinterpret_cast<const unsigned char*>(code)[i];hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}
            *output=7;return sink.Event("\"create\",\""+hex+"\"");
        }
        static void Release(void* raw,std::uintptr_t handle) noexcept
        {auto& sink=*static_cast<Sink*>(raw);sink.valid&=handle==7;sink.events.push_back("[\"release\",1]");}
        static std::int32_t Shader(void* raw,bool pixel,std::uintptr_t handle) noexcept
        {auto& sink=*static_cast<Sink*>(raw);sink.valid&=!pixel&&handle==7;return sink.Event("\"vertex-shader\",1");}
        static std::int32_t Constants(void* raw,bool pixel,const std::array<unsigned,4>* data,std::size_t rows) noexcept
        {auto& sink=*static_cast<Sink*>(raw);sink.valid&=!pixel&&rows==1;std::ostringstream out;out<<"\"vertex-constants\",0,";Array(out,data[0]);out<<','<<rows;return sink.Event(out.str());}
        static std::int32_t Draw(void* raw,bool indexed,const std::array<unsigned,6>& args) noexcept
        {auto& sink=*static_cast<Sink*>(raw);sink.valid&=indexed;std::ostringstream out;out<<"\"indexed\"";for(auto arg:args)out<<','<<arg;return sink.Event(out.str());}
    };
    std::string Run(const std::string& mode)
    {
        Sink sink;sink.fail=mode=="failed-device";auto manager=std::make_unique<spPCShaderManager>();
        auto effect=std::make_unique<spPCEffectTemplate>(1,"",2);spPCEffectTemplate::PassForAnalysis pass;
        pass.shaders[0].text[2]="Main";pass.shaders[0].text[3]="vs_2_0";effect->AppendPassForAnalysis(pass);
        Check(manager->SetFixedTemplateForAnalysis(effect)&&!effect,"prepared template ownership");
        spDXMaterial material;material.SetSpecularPowerForAnalysis(0);spDXShader::ConstantInputsForAnalysis inputs;inputs.material=&material;
        spPCEffectTemplate::CompilerStateForAnalysis compilerState;std::ostringstream requests;unsigned calls=0;
        const spPCEffectTemplate::CompilerForAnalysis compiler=[&](const auto& request)
        {
            ++calls;Check(compilerState.active&&!request.assembly,"native generation compiler state/stage");
            requests<<"[\"hlsl\",";Quoted(requests,request.source);requests<<",[";Quoted(requests,request.entry);requests<<',';Quoted(requests,request.target);requests<<"],0,[1]]";
            spPCEffectTemplate::CompilerOutputForAnalysis result;result.bytecode=std::vector<std::uint8_t>{0x10,0x20,0x30,0x40,0x50};result.reflection=std::vector<spPCEffectTemplate::ParameterForAnalysis>{{"MatDiffuse",0,1}};return result;
        };
        const spPCShaderGenerationForAnalysis generation{&compiler,&compilerState,&Sink::Create,&Sink::Release,&sink};
        std::ostringstream captures;captures<<'[';
        for(unsigned iteration=0;iteration<2;++iteration)
        {
            material.SetDiffuseColorForAnalysis({.25f,.5f,.75f,float(iteration)});sink.events.clear();
            const bool result=R::DrawAutomaticForAnalysis(sink.state,{2,11,13,17,19,0x803,0},*manager,inputs,0,nullptr,{},&Sink::Shader,&Sink::Constants,&Sink::Draw,&sink,&generation);
            Check(result,"whole generated automatic draw");if(iteration)captures<<',';captures<<'['<<iteration<<','<<unsigned(result)<<','<<sink.State()<<",[";
            bool first=true;for(const auto& event:sink.events){if(!first)captures<<',';first=false;captures<<event;}captures<<"]]";
        }
        captures<<']';Check(calls==1&&manager->GetCacheForAnalysis().size()==1,"second draw uses generated cache");
        sink.events.clear();manager.reset();Check(sink.valid,"device and generated-handle lifetime");
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<",["<<requests.str()<<"],"<<captures.str()<<",[";bool first=true;
        for(const auto& event:sink.events){if(!first)out<<',';first=false;out<<event;}out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        Run("generated");Run("failed-device");std::cout<<"PASS "<<checks<<'/'<<checks<<": automatic generated shader draw\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
