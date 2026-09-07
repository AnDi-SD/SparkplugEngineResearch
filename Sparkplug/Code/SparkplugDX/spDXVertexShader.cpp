#include "spDXVertexShader.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXVertexShader::ClassID,spDXShader::ClassID,"spDXVertexShader",&spDXShader::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXVertexShader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXVertexShader::vfunc_18() const noexcept{return Record;}
}
