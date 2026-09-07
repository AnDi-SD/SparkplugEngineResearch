#include "spPCVertexShader.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCVertexShader>();}
        const spRTTIRecord Record{spPCVertexShader::ClassID,spDXVertexShader::ClassID,"spPCVertexShader",&spDXVertexShader::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPCVertexShader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCVertexShader::vfunc_18() const noexcept{return Record;}
    spPCVertexShader::~spPCVertexShader(){if(deviceShader_&&release_)release_(deviceContext_,deviceShader_);}
    std::unique_ptr<spBaseObject> spPCVertexShader::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCVertexShader>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spPCVertexShader::CreateFromBytecodeForAnalysis(const std::uint32_t* code,std::uint32_t,
        CreateDeviceShaderForAnalysis create,ReleaseDeviceShaderForAnalysis release,void* context) noexcept
    {
        if(!create||!release)return false; // host callback lifetime guard
        release_=release;deviceContext_=context;(void)create(context,code,&deviceShader_);return true;
    }
}
