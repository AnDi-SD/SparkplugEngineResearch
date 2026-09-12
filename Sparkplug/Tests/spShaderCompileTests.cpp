#include "Code/SparkplugPC/spPCEffectTemplate.h"
#include "Code/SparkplugPC/spPCVertexShader.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::string Decode(const std::string& value)
    {if(value=="-")return {};std::string out;for(std::size_t i=0;i<value.size();i+=2)out.push_back(static_cast<char>(std::stoul(value.substr(i,2),nullptr,16)));return out;}
    void Quoted(std::ostream& out,std::string_view value)
    {
        out<<'"';for(unsigned char c:value)
        {if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32||c>=127)out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];else out<<c;}out<<'"';
    }
    std::string Run(const std::string& mode)
    {
        spPCEffectTemplate::ShaderForAnalysis record;std::string code,decl,header,insertion;unsigned hlsl,hresult;
        if(!(std::cin>>code>>decl>>header>>insertion>>hlsl>>hresult))throw std::runtime_error("input contract");
        record.text={Decode(code),Decode(decl),"Main","vs_2_0"};record.flags[2]=hlsl?0:1;
        auto readParameters=[]
        {
            unsigned count;std::cin>>count;std::vector<spPCEffectTemplate::ParameterForAnalysis> parameters;
            for(unsigned i=0;i<count;++i){std::string name;unsigned reg,rows;std::cin>>name>>reg>>rows;parameters.push_back({Decode(name),reg,rows});}
            return parameters;
        };
        record.parameters=readParameters();auto reflected=readParameters();
        spPCVertexShader shader;spPCEffectTemplate::CompilerStateForAnalysis state;std::ostringstream requests;
        const auto compiler=[&](const spPCEffectTemplate::CompilerRequestForAnalysis& request)
        {
            Check(state.active&&request.flags==0,"original compiler flag/zero options");
            requests<<'[';Quoted(requests,request.assembly?"assembly":"hlsl");requests<<',';Quoted(requests,request.source);requests<<",[";
            if(!request.assembly){Quoted(requests,request.entry);requests<<',';Quoted(requests,request.target);}
            requests<<"],"<<request.flags<<",["<<unsigned(state.active)<<"]]";
            spPCEffectTemplate::CompilerOutputForAnalysis result;result.hresult=static_cast<std::int32_t>(hresult);
            result.bytecode=std::vector<std::uint8_t>{0x10,0x20,0x30,0x40,0x50};result.reflection=reflected;return result;
        };
        const bool result=spPCEffectTemplate::CompileShaderForAnalysis(record,Decode(insertion),Decode(header),shader,compiler,state);
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<','<<unsigned(result)<<",["<<requests.str()<<"],[";bool first=true;
        for(const auto& parameter:shader.GetParametersForAnalysis())
        {if(!first)out<<',';first=false;out<<'[';Quoted(out,parameter.name.data());out<<','<<parameter.type<<','<<parameter.startRegister<<','<<parameter.registerCount<<']';}
        out<<"],"<<shader.GetScalarWordsForAnalysis()[8]<<",\"";
        for(const auto byte:shader.CompiledCodeForAnalysis())out<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];
        out<<"\",["<<unsigned(state.active)<<"]]";return out.str();
    }
    struct DeviceContext
    {
        bool failure=false,noOutput=false,valid=true;
        std::vector<std::string> events;
        static std::int32_t Create(void* raw,const std::uint32_t* code,std::uintptr_t* output) noexcept
        {
            auto& context=*static_cast<DeviceContext*>(raw);std::string hex;
            if(!code)context.valid=false;
            else for(unsigned i=0;i<5;++i){const auto byte=reinterpret_cast<const unsigned char*>(code)[i];hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}
            *output=context.noOutput?0:7;
            context.events.push_back("[\"create\",\""+hex+"\","+std::to_string(context.failure)+","+std::to_string(!context.noOutput)+"]");
            return context.failure?static_cast<std::int32_t>(0x80004005u):0;
        }
        static void Release(void* raw,std::uintptr_t handle) noexcept
        {auto& context=*static_cast<DeviceContext*>(raw);context.valid&=handle==7;context.events.push_back("[\"release\",1]");}
    };
    std::string RunGeneration(const std::string& mode)
    {
        spPCShaderManager::KeyForAnalysis key{};if(!(std::cin>>key[0]>>key[1]))throw std::runtime_error("generation key input");
        DeviceContext context;context.failure=mode.rfind("device-failed",0)==0;context.noOutput=mode=="device-failed";
        auto manager=std::make_unique<spPCShaderManager>();auto effect=std::make_unique<spPCEffectTemplate>(1,"",2);
        spPCEffectTemplate::PassForAnalysis pass;pass.shaders[0].text={"void Main(){// INSERTION POINT\n}\n","DECL\n","Main","vs_2_0"};
        effect->AppendPassForAnalysis(pass);Check(manager->SetFixedTemplateForAnalysis(effect)&&!effect,"owned prepared template transferred");
        spPCEffectTemplate::CompilerStateForAnalysis state;std::ostringstream requests;unsigned calls=0;
        const auto compiler=[&](const spPCEffectTemplate::CompilerRequestForAnalysis& request)
        {
            ++calls;requests<<'[';Quoted(requests,request.assembly?"assembly":"hlsl");requests<<',';Quoted(requests,request.source);requests<<",[";
            Quoted(requests,request.entry);requests<<',';Quoted(requests,request.target);requests<<"],"<<request.flags<<",["<<unsigned(state.active)<<"]]";
            spPCEffectTemplate::CompilerOutputForAnalysis result;result.bytecode=std::vector<std::uint8_t>{0x10,0x20,0x30,0x40,0x50};result.reflection=std::vector<spPCEffectTemplate::ParameterForAnalysis>{{"MatDiffuse",4,1}};return result;
        };
        const auto first=manager->SelectOrCreateForAnalysis(key,compiler,state,&DeviceContext::Create,&DeviceContext::Release,&context);
        Check(first.completed&&first.shader,"whole generated selection completes");
        const auto second=manager->SelectOrCreateForAnalysis(key,compiler,state,&DeviceContext::Create,&DeviceContext::Release,&context);
        const auto none=manager->SelectOrCreateForAnalysis({key[0]&~15u,key[1]},compiler,state,&DeviceContext::Create,&DeviceContext::Release,&context);
        Check(calls==1&&second.shader==first.shader&&none.completed&&!none.shader,"generated cache hit and zero-weight bypass");
        auto* shader=first.shader;std::ostringstream states;
        states<<"[1,"<<unsigned(second.shader==shader)<<','<<unsigned(!none.shader)<<','<<manager->GetCacheForAnalysis().size()<<','<<unsigned(shader->GetDeviceShaderForAnalysis()!=0)<<','<<shader->GetScalarWordsForAnalysis()[8]<<",[";bool initial=true;
        for(const auto& parameter:shader->GetParametersForAnalysis())
        {if(!initial)states<<',';initial=false;states<<'[';Quoted(states,parameter.name.data());states<<','<<parameter.type<<','<<parameter.startRegister<<','<<parameter.registerCount<<']';}
        states<<"],\"";for(const auto byte:shader->CompiledCodeForAnalysis())states<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];states<<"\"]";
        manager.reset();Check(context.valid,"device receives compiled code and releases the returned handle");
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<",["<<key[0]<<','<<key[1]<<"],["<<requests.str()<<"],"<<states.str()<<",[";
        initial=true;for(const auto& event:context.events){if(!initial)out<<',';initial=false;out<<event;}out<<"]]";return out.str();
    }
    // Own research transport. Source composition and key decoding stay in the
    // shared recovered classes; the caller supplies the original RFX record.
    void SourceBatch()
    {
        std::string code,decl,entry,target;unsigned count=0;
        if(!(std::cin>>code>>decl>>entry>>target>>count)||count>512||
            code.size()>65536||decl.size()>16384||entry.size()>256||target.size()>64)
            throw std::runtime_error("bounded source batch input");
        spPCEffectTemplate::ShaderForAnalysis record;
        record.text={Decode(code),Decode(decl),Decode(entry),Decode(target)};
        std::cout<<'[';
        for(unsigned i=0;i<count;++i)
        {
            spPCShaderManager::KeyForAnalysis key{};
            if(!(std::cin>>key[0]>>key[1]))throw std::runtime_error("source batch key input");
            const auto input=spPCShaderManager::BuildSourceInputsForAnalysis(key);
            const auto source=spPCEffectTemplate::BuildShaderSourceForAnalysis(record,input.insertion,input.header);
            if(i)std::cout<<',';
            std::cout<<"{\"key\":["<<key[0]<<','<<key[1]<<"],\"managerSelectsShader\":"<<((key[0]&15)?"true":"false")<<",\"source\":";
            Quoted(std::cout,source);std::cout<<",\"entry\":";Quoted(std::cout,record.text[2]);
            std::cout<<",\"target\":";Quoted(std::cout,record.text[3]);std::cout<<'}';
        }
        std::cout<<"]\n";
    }
    void SourceRecord()
    {
        std::string code,decl,entry,target,insertion,header;
        if(!(std::cin>>code>>decl>>entry>>target>>insertion>>header)||code.size()>65536||
            decl.size()>16384||insertion.size()>16384||header.size()>16384||entry.size()>256||target.size()>64)
            throw std::runtime_error("bounded source record input");
        spPCEffectTemplate::ShaderForAnalysis record;
        record.text={Decode(code),Decode(decl),Decode(entry),Decode(target)};
        Quoted(std::cout,spPCEffectTemplate::BuildShaderSourceForAnalysis(record,Decode(insertion),Decode(header)));
        std::cout<<'\n';
    }
    void ParameterTypes()
    {
        unsigned count=0;if(!(std::cin>>count)||count>256)throw std::runtime_error("bounded parameter names");
        std::cout<<'[';
        for(unsigned i=0;i<count;++i)
        {
            std::string name;if(!(std::cin>>name)||name.size()>128)throw std::runtime_error("parameter name input");
            name=Decode(name);if(i)std::cout<<',';std::cout<<"{\"name\":";Quoted(std::cout,name);
            std::cout<<",\"type\":"<<spDXShader::LookupParameterTypeForAnalysis(name)<<'}';
        }
        std::cout<<"]\n";
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--source-batch"){SourceBatch();return 0;}
        if(argc==2&&std::string(argv[1])=="--source-record"){SourceRecord();return 0;}
        if(argc==2&&std::string(argv[1])=="--parameter-types"){ParameterTypes();return 0;}
        if(argc==3&&std::string(argv[1])=="--input"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--generation"){std::cout<<RunGeneration(argv[2])<<'\n';return 0;}
        spPCEffectTemplate::ShaderForAnalysis record;record.flags[2]=1;record.text[0]="A// INSERTION POINT B// INSERTION POINT";record.text[1]="D";
        Check(spPCEffectTemplate::BuildShaderSourceForAnalysis(record,"I","H")=="HDAI// INSERTION POINT B// INSERTION POINT","first marker is preserved with insertion before it");
        record.parameters.push_back({"MatDiffuse",4,1});spPCVertexShader shader;spPCEffectTemplate::CompilerStateForAnalysis state;
        const auto compiler=[&](const auto& request)
        {Check(state.active&&request.assembly,"assembly compiler selection/activity");spPCEffectTemplate::CompilerOutputForAnalysis output;output.hresult=-1;output.bytecode=std::vector<std::uint8_t>{1,2,3};return output;};
        Check(spPCEffectTemplate::CompileShaderForAnalysis(record,"I","H",shader,compiler,state),"returned code buffer drives success despite failed HRESULT");
        Check(!state.active&&shader.CompiledCodeForAnalysis()==std::vector<std::uint8_t>{1,2,3},"owned code and cleared activity");
        Check(shader.GetParametersForAnalysis()[0].type==8&&shader.GetScalarWordsForAnalysis()[8]==1,"original parameter append/name resolution");
        const auto failed=[](const auto&){spPCEffectTemplate::CompilerOutputForAnalysis output;output.bytecode.reset();return output;};
        Check(!spPCEffectTemplate::CompileShaderForAnalysis(record,"","",shader,failed,state)&&state.active,"analytical null-code failure leaves original activity state");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": shader source and compiler output slice\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
