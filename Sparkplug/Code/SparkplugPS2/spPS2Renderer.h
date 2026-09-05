#pragma once

// Inferred platform header path. The basename spPS2Renderer.cpp survives in
// PS2 diagnostics; no full original source/header path is present.

#include "../Sparkplug/spRenderer.h"

namespace sparkplug::reconstruction
{
    class spPS2Renderer final : public spRenderer
    {
    public:
        static constexpr spClassID ClassID = 0x303652B8;
        static constexpr std::size_t TextureStateCacheCount = 96;

        spPS2Renderer();
        ~spPS2Renderer() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
