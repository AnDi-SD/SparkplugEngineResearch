#include "Code/SparkplugPC/spPCVertexShader.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    std::string Snapshot(const spPCVertexShader& shader)
    {
        std::ostringstream out;out<<"[[";bool first=true;for(auto word:shader.GetScalarWordsForAnalysis())
        {if(!first)out<<',';first=false;out<<word;}out<<"],"<<shader.GetDeviceShaderForAnalysis()<<']';return out.str();
    }
    struct Sink
    {
        std::string mode;unsigned ordinal=0;std::vector<std::string> events;
        static std::int32_t Create(void* ptr,const std::uint32_t* code,std::uintptr_t* output) noexcept
        {
            auto& s=*static_cast<Sink*>(ptr);const auto before=*output;const bool failure=s.mode=="failed"||s.mode=="failed-output";
            *output=s.mode=="failed"?0:++s.ordinal;std::ostringstream event;event<<"[\"create\",[";
            for(unsigned i=0;i<4;++i){if(i)event<<',';event<<code[i];}event<<"],"<<before<<','<<*output<<','<<failure<<']';s.events.push_back(event.str());return failure?-1:0;
        }
        static void Release(void* ptr,std::uintptr_t handle) noexcept
        {auto& s=*static_cast<Sink*>(ptr);s.events.push_back("[\"release\","+std::to_string(handle)+']');}
    };
    std::string Run(const std::string& mode)
    {
        Sink sink;sink.mode=mode;sink.events.reserve(4);std::ostringstream out;out<<"[\""<<mode<<"\",[";
        auto shader=std::make_unique<spPCVertexShader>();Check(shader->IsKindOf(spDXShader::ClassID),"actual shader ancestry");out<<Snapshot(*shader);
        if(mode=="clone")
        {
            spDXShader::ScalarWordsForAnalysis words{};words[0]=0x12345678;words[8]=7;shader->SetScalarWordsForAnalysis(words);out<<','<<Snapshot(*shader);
            auto owner=shader->Clone();auto* clone=dynamic_cast<spPCVertexShader*>(owner.get());
            Check(clone&&clone->GetScalarWordsForAnalysis()==spDXShader::ScalarWordsForAnalysis{}&&!clone->GetDeviceShaderForAnalysis(),"original name-only clone leaves shader state blank");out<<','<<Snapshot(*clone);
        }
        else if(mode!="factory")
        {
            const std::array<std::uint32_t,4> code{0xfffe0101,1,0x80000000,0xffff};
            const std::vector<unsigned> unused=mode=="repeat"?std::vector<unsigned>{0,0xffffffff}:std::vector<unsigned>{0x12345678};
            for(auto arg:unused)
            {
                const bool result=shader->CreateFromBytecodeForAnalysis(code.data(),arg,Sink::Create,Sink::Release,&sink);
                Check(result,"original ignores device HRESULT");out<<",["<<arg<<','<<result<<','<<Snapshot(*shader)<<']';
            }
        }
        shader.reset();out<<"],[";bool first=true;for(const auto& event:sink.events){if(!first)out<<',';first=false;out<<event;}out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"factory","clone","create","failed","failed-output","repeat"})Run(mode);
            spPCVertexShader shader;shader.SetName("shader-name");auto owner=shader.Clone();auto* copy=dynamic_cast<spPCVertexShader*>(owner.get());
            Check(copy&&std::string(copy->GetName())=="shader-name","inherited name copy preserved");
            Check(!shader.CreateFromBytecodeForAnalysis(nullptr,0,nullptr,nullptr,nullptr),"host missing-device guard");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": vertex shader slice\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
