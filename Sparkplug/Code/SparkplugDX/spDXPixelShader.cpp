#include "spDXPixelShader.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXPixelShader::ClassID,spDXShader::ClassID,"spDXPixelShader",&spDXShader::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXPixelShader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXPixelShader::vfunc_18() const noexcept{return Record;}
}
