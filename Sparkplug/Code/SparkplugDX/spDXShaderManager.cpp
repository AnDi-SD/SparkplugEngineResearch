#include "spDXShaderManager.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXShaderManager::ClassID,spBaseObject::ClassID,"spDXShaderManager",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXShaderManager::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXShaderManager::vfunc_18() const noexcept{return Record;}
}
