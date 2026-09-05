#include "spDXRenderer.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord DXRendererRecord{
            spDXRenderer::ClassID,
            spRenderer::ClassID,
            "spDXRenderer",
            &spRenderer::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool DXRendererRegistered =
            spRTTIManager::Instance().Register(DXRendererRecord);
    }

    const spRTTIRecord& spDXRenderer::StaticRTTI() noexcept
    {
        (void)DXRendererRegistered;
        return DXRendererRecord;
    }
}
