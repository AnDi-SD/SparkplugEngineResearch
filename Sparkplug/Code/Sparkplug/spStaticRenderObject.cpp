#include "spStaticRenderObject.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spStaticRenderObject>(); }
        const spRTTIRecord Record{spStaticRenderObject::ClassID, spNamedObject::ClassID,
            "spStaticRenderObject", &spNamedObject::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spStaticRenderObject::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& spStaticRenderObject::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spStaticRenderObject::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spStaticRenderObject>();
        manager.RegisterClone(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
}
