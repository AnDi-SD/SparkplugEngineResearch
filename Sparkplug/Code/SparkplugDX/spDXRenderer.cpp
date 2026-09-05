#include "spDXRenderer.h"

namespace sparkplug::reconstruction
{
    spDXRenderer::spDXRenderer()
        : spRenderer(TextureStateCacheCount)
    {
    }

    spDXRenderer::~spDXRenderer() = default;

    std::unique_ptr<spBaseObject> spDXRenderer::vfunc_10(
        spCloneManager&) const
    {
        // Native spDXRenderer remains abstract and has no RTTI factory.
        return nullptr;
    }

    const spRTTIRecord& spDXRenderer::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
}
