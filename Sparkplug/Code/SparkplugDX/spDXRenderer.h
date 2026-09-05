#pragma once

// Inferred header path. The exact surviving implementation path is
// Z:\Sparkplug\Code\SparkplugDX\spDXRenderer_Init.cpp.

#include "../Sparkplug/spRenderer.h"

namespace sparkplug::reconstruction
{
    class spDXRenderer : public spRenderer
    {
    public:
        static constexpr spClassID ClassID = 0x46004EE1;
        static constexpr std::size_t TextureStateCacheCount = 72;

        ~spDXRenderer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

    protected:
        spDXRenderer();
    };
}
