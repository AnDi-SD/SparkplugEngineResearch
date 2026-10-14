// Source-side captures are compared with fresh original PC instruction cases.
// Compiler/device callbacks are explicit external providers, not game code.
#include "Code/SparkplugPC/spPCPixelShader.h"
#include "Code/SparkplugPC/spPCEffectTemplate.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
    std::string Snapshot(const spPCPixelShader& shader)
    {
        std::ostringstream out;out<<"[[";bool first=true;
        for(auto word:shader.GetScalarWordsForAnalysis()){if(!first)out<<',';first=false;out<<word;}
        out<<"],"<<shader.GetParametersForAnalysis().size()<<','<<shader.CompiledCodeForAnalysis().size()
           <<','<<shader.GetDeviceShaderForAnalysis()<<']';return out.str();
    }
    struct Sink
    {
        std::string mode;unsigned ordinal=0;bool valid=true;
        spPCPixelShader* shader=nullptr;
        const std::uint32_t* expectedCode=nullptr;
        std::vector<std::string> events;
        static std::int32_t Create(void* raw,const std::uint32_t* code,std::uintptr_t* output) noexcept
        {
            auto& sink=*static_cast<Sink*>(raw);sink.valid&=code==sink.expectedCode;
            const auto before=*output;const bool failure=sink.mode.rfind("failed-",0)==0;
            const bool write=(sink.mode!="failed-unwritten"&&sink.mode!="repeat-unwritten")||
                (sink.ordinal==0&&sink.mode=="repeat-unwritten");
            if(write)*output=sink.mode=="failed-null"?0:sink.ordinal+1;
            ++sink.ordinal;std::ostringstream out;out<<"[\"create\",[";
            for(unsigned i=0;i<4;++i){if(i)out<<',';out<<code[i];}
            out<<"],"<<before<<','<<*output<<','<<unsigned(failure)<<','<<unsigned(write)<<']';
            sink.events.push_back(out.str());return failure?static_cast<std::int32_t>(0x80004005u):0;
        }
        static void Release(void* raw,std::uintptr_t handle) noexcept
        {
            auto& sink=*static_cast<Sink*>(raw);std::ostringstream out;
            out<<"[\"release\","<<handle<<','
               <<(sink.shader->CompiledCodeForAnalysis().empty()?"false":"true")<<','
               <<(sink.shader->GetParametersForAnalysis().empty()?"false":"true")<<']';
            sink.events.push_back(out.str());
        }
    };
    void PopulateStorage(spPCPixelShader& shader)
    {
        shader.SetCompiledCodeForAnalysis({'P','S','-','c','o','d','e'});
        shader.SetParametersForAnalysis({spDXShader::ParameterForAnalysis{}});
    }
    std::string Run(const std::string& mode)
    {
        Check(mode=="factory"||mode=="clone"||mode=="create"||mode=="failed-null"||mode=="failed-write"||
              mode=="failed-unwritten"||mode=="repeat-write"||mode=="repeat-unwritten"||mode=="teardown","explicit operation");
        Sink sink;sink.mode=mode;sink.events.reserve(4);
        auto shader=std::make_unique<spPCPixelShader>();sink.shader=shader.get();
        Check(shader->IsKindOf(spDXPixelShader::ClassID)&&shader->IsKindOf(spDXShader::ClassID),"pixel shader ancestry");
        Check(sink.events.empty(),"factory has no external device lifetime calls");
        std::ostringstream out;out<<"[\""<<mode<<"\",["<<Snapshot(*shader);
        if(mode=="clone")
        {
            spDXShader::ScalarWordsForAnalysis words{};words[0]=0x12345678;words[8]=7;
            shader->SetScalarWordsForAnalysis(words);PopulateStorage(*shader);out<<','<<Snapshot(*shader);
            auto owner=shader->Clone();auto* clone=dynamic_cast<spPCPixelShader*>(owner.get());
            Check(clone&&clone->GetScalarWordsForAnalysis()==spDXShader::ScalarWordsForAnalysis{}&&
                  clone->GetParametersForAnalysis().empty()&&clone->CompiledCodeForAnalysis().empty()&&
                  !clone->GetDeviceShaderForAnalysis(),"named clone excludes all shader-owned storage");
            out<<','<<Snapshot(*clone);
        }
        else if(mode!="factory")
        {
            const std::array<std::uint32_t,4> code{0xffff0101,1,0x80000000,0xffff};sink.expectedCode=code.data();
            for(unsigned i=0;i<(mode.rfind("repeat-",0)==0?2u:1u);++i)
            {
                spDXPixelShader& abstract=*shader;
                const bool result=abstract.CreateFromBytecodeForAnalysis(code.data(),Sink::Create,Sink::Release,&sink);
                Check(result,"native AL1 ignores external HRESULT");out<<",["<<unsigned(result)<<','<<Snapshot(*shader)<<']';
            }
            Check(sink.events.size()==sink.ordinal,"creation never releases overwritten handle");
            if(mode=="teardown"){PopulateStorage(*shader);out<<','<<Snapshot(*shader);}
        }
        shader.reset();Check(sink.valid,"external device received original caller bytecode pointer");
        out<<"],[";bool first=true;for(const auto& event:sink.events){if(!first)out<<',';first=false;out<<event;}out<<"]]";
        return out.str();
    }
    void ConnectedSharedStorage()
    {
        // Shared original compiler-output/descriptor consumer accepts a pixel
        // shader through spDXShader. This regression is not a live SDK/GPU test.
        Sink sink;sink.mode="create";sink.events.reserve(2);
        auto shader=std::make_unique<spPCPixelShader>();sink.shader=shader.get();
        spPCEffectTemplate::ShaderForAnalysis record;
        record.text={"body // INSERTION POINT", "decl ", "Main", "ps_2_0"};
        spPCEffectTemplate::CompilerStateForAnalysis state;unsigned calls=0;
        const std::array<std::uint32_t,4> words{0xffff0200,1,0x80000000,0xffff};
        const auto compiler=[&](const spPCEffectTemplate::CompilerRequestForAnalysis& request)
        {
            ++calls;Check(state.active&&!request.assembly&&request.flags==0&&request.target=="ps_2_0"&&
                          request.entry=="Main"&&request.source=="head decl body insert// INSERTION POINT","shared compiler request preserves pixel target/source");
            spPCEffectTemplate::CompilerOutputForAnalysis output;
            output.hresult=static_cast<std::int32_t>(0x80004005u); // Native consumes supplied code despite failed HRESULT.
            output.bytecode=std::vector<std::uint8_t>(sizeof(words));
            std::memcpy(output.bytecode->data(),words.data(),sizeof(words));
            output.reflection=std::vector<spPCEffectTemplate::ParameterForAnalysis>{{"ConstColor",0,1}};
            return output;
        };
        Check(spPCEffectTemplate::CompileShaderForAnalysis(record,"insert","head ",*shader,compiler,state)&&
              calls==1&&!state.active&&shader->GetScalarWordsForAnalysis()[8]==1,"compiler owns bytecode and resolved pixel descriptor");
        spDXShader::ConstantInputsForAnalysis inputs;inputs.constantColor=0xff804020;
        std::vector<std::uint32_t> constants;
        Check(shader->BuildFullyWrittenConstantsForAnalysis(constants,inputs,1)&&
              constants==std::vector<std::uint32_t>{0x3f008081,0x3e808081,0x3e008081,0x3f800000},"pixel descriptors use shared original constant-color storage");
        sink.expectedCode=reinterpret_cast<const std::uint32_t*>(shader->CompiledCodeForAnalysis().data());
        Check(shader->CreateFromBytecodeForAnalysis(sink.expectedCode,Sink::Create,Sink::Release,&sink),"generated pixel bytes reach external device");
        shader.reset();Check(sink.valid&&sink.events.size()==2&&sink.events.back()=="[\"release\",1,true,true]","device released while code and descriptors still owned");
    }
    void CloneLive()
    {
        Sink sink;sink.mode="create";sink.events.reserve(2);
        const std::array<std::uint32_t,4> code{0xffff0101,1,0x80000000,0xffff};sink.expectedCode=code.data();
        auto shader=std::make_unique<spPCPixelShader>();sink.shader=shader.get();shader->SetName("pixel-name");
        Check(shader->CreateFromBytecodeForAnalysis(code.data(),Sink::Create,Sink::Release,&sink),"live clone input");
        auto copy=shader->Clone();auto* clone=dynamic_cast<spPCPixelShader*>(copy.get());
        Check(clone&&std::string(clone->GetName())=="pixel-name"&&!clone->GetDeviceShaderForAnalysis(),"inherited name copy keeps device ownership exclusive");
        copy.reset();Check(sink.events.size()==1,"blank clone does not release original device handle");
        shader.reset();Check(sink.events.size()==2,"original releases its device handle once");
    }
}
int main(int argc,char** argv)
{
    try
    {
        static_assert(std::is_abstract_v<spDXPixelShader>);
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"factory","clone","create","failed-null","failed-write","failed-unwritten","repeat-write","repeat-unwritten","teardown"})Run(mode);
            auto created=spRTTIManager::Instance().Create(spPCPixelShader::ClassID);
            Check(created&&created->IsExactly(spPCPixelShader::ClassID),"registered concrete pixel factory");
            Check(!spRTTIManager::Instance().Create(spDXPixelShader::ClassID),"abstract pixel registration has no factory");
            spPCPixelShader missing;
            Check(!missing.CreateFromBytecodeForAnalysis(nullptr,nullptr,nullptr,nullptr),"own host missing-provider guard");
            CloneLive();ConnectedSharedStorage();
            std::cout<<"PASS "<<checks<<'/'<<checks<<": pixel shader native-operation captures and shared storage pipeline\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
