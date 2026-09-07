#include "spPCEffectTemplate.h"
#include "spPCRFXFileLoader.h"
#include "../SparkplugDX/spDXShader.h"
#include <cstring>
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spPCEffectTemplate::ClassID,spBaseObject::ClassID,"spPCEffectTemplate",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spPCEffectTemplate::spPCEffectTemplate(std::uint32_t identity,std::string document,std::uint32_t kind)
        :identity_(identity),kind_(kind),document_(std::move(document)){}
    const spRTTIRecord& spPCEffectTemplate::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCEffectTemplate::vfunc_18() const noexcept{return Record;}
    void spPCEffectTemplate::ShaderForAnalysis::ResetForAnalysis()
    {for(auto& value:text)value.clear();flags={};} //4CFF40 leaves parameter vector intact.
    std::string spPCEffectTemplate::BuildShaderSourceForAnalysis(const ShaderForAnalysis& shader,std::string_view insertion,std::string_view header)
    {
        std::string source(header);source+=shader.text[1];source+=shader.text[0];
        const auto position=source.find("// INSERTION POINT");
        if(position!=std::string::npos&&!insertion.empty())source.insert(position,insertion.data(),insertion.size());
        return source;
    }
    bool spPCEffectTemplate::CompileShaderForAnalysis(const ShaderForAnalysis& shader,std::string_view insertion,std::string_view header,
        spDXShader& output,const CompilerForAnalysis& compiler,CompilerStateForAnalysis& state)
    {
        if(!compiler)return false;
        CompilerRequestForAnalysis request;request.assembly=shader.flags[2]!=0;
        request.source=BuildShaderSourceForAnalysis(shader,insertion,header);
        if(!request.assembly){request.entry=shader.text[2];request.target=shader.text[3];}
        state.active=true;const auto result=compiler(request);
        const auto append=[&](const std::vector<ParameterForAnalysis>& parameters)
        {
            for(const auto& parameter:parameters)
            {
                if(parameter.name.size()>=32)return false; // Protect native32-byte stack field.
                spDXShader::ParameterForAnalysis converted;
                std::memcpy(converted.name.data(),parameter.name.c_str(),parameter.name.size()+1);
                converted.startRegister=parameter.startRegister;converted.registerCount=parameter.registerCount;
                if(!output.AppendParameterForAnalysis(converted))return false;
            }
            return true;
        };
        if(request.assembly){if(!append(shader.parameters))return false;}
        else
        {
            if(!result.bytecode||!result.reflection)return false;
            if(!append(*result.reflection))return false;
        }
        // Original null-code branches report diagnostics and leave active set;
        // analytical false does not claim that unresolved error-object path.
        if(!result.bytecode)return false;
        state.active=false;output.SetCompiledCodeForAnalysis(*result.bytecode);return true;
    }
    bool spPCEffectTemplate::InitializeFromEventsForAnalysis(const XmlEventsForAnalysis* events)
    {
        if(kind_==2)
        {
            if(!events)return false;
            spPCRFXFileLoader loader;loader.BindForAnalysis(*this);
            for(const auto& event:*events)if(!loader.ConsumeForAnalysis(event))return false;
        }
        ready_=true;return true;
    }
}
