#include "spPCPixelShader.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCPixelShader>();}
        const spRTTIRecord Record{spPCPixelShader::ClassID,spDXPixelShader::ClassID,"spPCPixelShader",&spDXPixelShader::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPCPixelShader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCPixelShader::vfunc_18() const noexcept{return Record;}
    spPCPixelShader::~spPCPixelShader()
    {
        if(deviceShader_&&release_)release_(deviceContext_,deviceShader_);
        deviceShader_=0; // PC4C9E88, before inherited shader storage teardown.
    }
    std::unique_ptr<spBaseObject> spPCPixelShader::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCPixelShader>();manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spPCPixelShader::CreateFromBytecodeForAnalysis(const std::uint32_t* code,
        CreateDeviceShaderForAnalysis create,ReleaseDeviceShaderForAnalysis release,void* context) noexcept
    {
        if(!create||!release)return false; // Own host guard, not native behavior.
        release_=release;deviceContext_=context;
        (void)create(context,code,&deviceShader_);
        return true; // Original returns AL1 even on device failure/unchanged output.
    }
}
