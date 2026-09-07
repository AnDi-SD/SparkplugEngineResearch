#include "spPCShaderManager.h"
#include <algorithm>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCShaderManager>();}
        const spRTTIRecord Record{spPCShaderManager::ClassID,spDXShaderManager::ClassID,"spPCShaderManager",&spDXShaderManager::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPCShaderManager::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCShaderManager::vfunc_18() const noexcept{return Record;}
    bool spPCShaderManager::KeyLessForAnalysis::operator()(const KeyForAnalysis& a,const KeyForAnalysis& b) const noexcept
    {
        // Native repe cmpsb compares the eight LITTLE-ENDIAN bytes, not u64.
        for(unsigned word=0;word<2;++word)for(unsigned byte=0;byte<4;++byte)
        {auto x=(a[word]>>(byte*8))&255u,y=(b[word]>>(byte*8))&255u;if(x!=y)return x<y;}
        return false;
    }
    spPCShaderManager::SelectionForAnalysis spPCShaderManager::SelectCachedForAnalysis(const KeyForAnalysis& key) const noexcept
    {
        if(!(key[0]&15))return {true,nullptr};
        const auto found=cache_.find(key);return found==cache_.end()?SelectionForAnalysis{}:SelectionForAnalysis{true,found->second.get()};
    }
    bool spPCShaderManager::CacheShaderForAnalysis(const KeyForAnalysis& key,std::unique_ptr<spPCVertexShader>& shader)
    {
        if(cache_.find(key)!=cache_.end())return false;
        cache_.emplace(key,std::move(shader));return true;
    }
    bool spPCShaderManager::SetFixedTemplateForAnalysis(std::unique_ptr<spPCEffectTemplate>& value)
    {if(fixedTemplate_||!value)return false;fixedTemplate_=std::move(value);return true;}
    spPCShaderManager::SourceInputsForAnalysis spPCShaderManager::BuildSourceInputsForAnalysis(const KeyForAnalysis& key)
    {
        SourceInputsForAnalysis input;const auto mask=key[0];
        const auto line=[](std::string_view name,std::uint32_t value){return std::string(name)+" = "+std::to_string(value)+";\n";};
        input.insertion=line("BlendWeightCount",mask&15)+line("ColorMode",(mask>>16)&15)+line("LightCount",(mask>>20)&15)+line("bUseSpecular",(mask>>24)&1);
        for(unsigned i=0;i<((mask>>20)&15);++i)input.insertion+=line("LightType["+std::to_string(i)+"]",(key[1]>>(2*i))&3);
        for(unsigned i=0;i<8;++i)input.insertion+=line("bHasUVTransform["+std::to_string(i)+"]",(mask>>(8+i))&1);
        for(unsigned i=0;i<std::min((mask>>4)&15u,8u);++i)input.header+="#define USE_TEXCOORD"+std::to_string(i)+"\n";
        return input;
    }
    spPCShaderManager::SelectionForAnalysis spPCShaderManager::SelectOrCreateForAnalysis(const KeyForAnalysis& key,
        const spPCEffectTemplate::CompilerForAnalysis& compiler,spPCEffectTemplate::CompilerStateForAnalysis& state,
        spPCVertexShader::CreateDeviceShaderForAnalysis create,spPCVertexShader::ReleaseDeviceShaderForAnalysis release,void* context)
    {
        const auto existing=SelectCachedForAnalysis(key);if(existing.completed)return existing;
        if(!fixedTemplate_||fixedTemplate_->PassesForAnalysis().empty()||!compiler||!create||!release)return {};
        const auto input=BuildSourceInputsForAnalysis(key);auto shader=std::make_unique<spPCVertexShader>();
        // Native4C8CCB ignores the compile return. Unresolved null-code/error
        // paths are guarded here until their whole manager behavior is known.
        if(!spPCEffectTemplate::CompileShaderForAnalysis(fixedTemplate_->PassesForAnalysis()[0].shaders[0],input.insertion,input.header,*shader,compiler,state))return {};
        if(!shader->CreateFromBytecodeForAnalysis(reinterpret_cast<const std::uint32_t*>(shader->CompiledCodeForAnalysis().data()),0,create,release,context))return {};
        auto* result=shader.get();if(!CacheShaderForAnalysis(key,shader))return {}; // Reentrant insertion remains open.
        return {true,result};
    }
    std::unique_ptr<spBaseObject> spPCShaderManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCShaderManager>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr; // actual copy40ECE0 no-op
    }
}
