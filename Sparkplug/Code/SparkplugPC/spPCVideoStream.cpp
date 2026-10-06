#include "spPCVideoStream.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spPCVideoStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spVideoStream::ClassID, "spPCVideoStream",
            &spVideoStream::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spPCVideoStream>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    const spRTTIRecord& spPCVideoStream::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spPCVideoStream::vfunc_10(spCloneManager& manager) const
    {
        // PC4C73B0 operates on secondary this+4 and invokes Named413120.
        auto clone = std::make_unique<spPCVideoStream>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spPCVideoStream::NativeHook00ForAnalysis() noexcept
    {
        // PC4C7320: virtual primary+04 call, followed by MOV AL1.
        NativeHook04ForAnalysis(); return true;
    }
}
