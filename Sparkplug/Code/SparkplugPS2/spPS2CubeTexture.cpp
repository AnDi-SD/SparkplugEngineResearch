#include "spPS2CubeTexture.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spPS2CubeTexture>(); }
        const spRTTIRecord Record{spPS2CubeTexture::ClassID, spCubeTexture::ClassID,
            "spPS2CubeTexture", &spCubeTexture::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPS2CubeTexture::StaticRTTI() noexcept
    { (void)Registered; return Record; }
    const spRTTIRecord& spPS2CubeTexture::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spPS2CubeTexture::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2CubeTexture>(); manager.RegisterCloneForAnalysis(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spPS2CubeTexture::InitCubeBufferStateForAnalysis(std::uint32_t width, std::uint32_t height,
        std::uint32_t field18, std::uint32_t flags, bool normalize) noexcept
    {
        const auto plan = ApplyBufferStateForAnalysis(width, height, 0, flags, normalize);
        ApplyNativeMipStateForAnalysis(plan.effectiveWidth, plan.effectiveHeight, field18, flags, 0);
        return NativeHook35E0ForAnalysis();
    }
}
