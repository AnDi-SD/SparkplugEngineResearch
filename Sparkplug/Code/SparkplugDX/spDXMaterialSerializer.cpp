#include "spDXMaterialSerializer.h"
#include <utility>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXMaterialSerializer>();}
        const spRTTIRecord Record{spDXMaterialSerializer::ClassID,spMaterialSerializer::ClassID,"spDXMaterialSerializer",&spMaterialSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXMaterialSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXMaterialSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spDXMaterialSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXMaterialSerializer>();manager.RegisterClone(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
