#include "spPS2VideoStream.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spPS2VideoStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spVideoStream::ClassID, "spPS2VideoStream",
            &spVideoStream::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spPS2VideoStream>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    const spRTTIRecord& spPS2VideoStream::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spPS2VideoStream::vfunc_10(spCloneManager& manager) const
    {
        // Original20D740 allocates/constructs a fresh primary object, registers
        // source/destination secondary views+4, then calls Named copy105DC0.
        auto clone = std::make_unique<spPS2VideoStream>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
}
