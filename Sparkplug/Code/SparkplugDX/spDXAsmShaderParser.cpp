#include "spDXAsmShaderParser.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create()
        {
            return std::make_unique<spDXAsmShaderParser>();
        }
        const spRTTIRecord Record{
            spDXAsmShaderParser::ClassID,spBaseObject::ClassID,
            "spDXAsmShaderParser",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=
            spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }

    const spRTTIRecord& spDXAsmShaderParser::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }

    const spRTTIRecord& spDXAsmShaderParser::vfunc_18() const noexcept
    {
        return Record;
    }

    std::unique_ptr<spBaseObject> spDXAsmShaderParser::vfunc_10(
        spCloneManager& manager) const
    {
        // PC004CFB20 calls factory004CFAB0, registers the pair, then invokes
        // inherited BaseObject Copy. There are no assembly/text/code fields
        // copied here and no shader compilation is performed by this method.
        auto clone=std::make_unique<spDXAsmShaderParser>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
