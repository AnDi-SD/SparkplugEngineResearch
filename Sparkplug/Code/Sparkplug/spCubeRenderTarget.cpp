#include "spCubeRenderTarget.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord CubeRenderTargetRecord{
            spCubeRenderTarget::ClassID,
            spRenderTarget::ClassID,
            "spCubeRenderTarget",
            &spRenderTarget::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool CubeRenderTargetRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(CubeRenderTargetRecord);
    }

    spCubeRenderTarget::~spCubeRenderTarget() = default;

    const spRTTIRecord& spCubeRenderTarget::StaticRTTI() noexcept
    {
        (void)CubeRenderTargetRegistered;
        return CubeRenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spCubeRenderTarget::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spCubeRenderTarget::vfunc_18() const noexcept
    {
        return CubeRenderTargetRecord;
    }
}
