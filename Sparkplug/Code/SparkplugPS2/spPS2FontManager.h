#pragma once

// Inferred header/module path; no original PS2 source-path string survives.

#include "../Sparkplug/spFontManager.h"

namespace sparkplug::reconstruction
{
    class spPS2FontManager final : public spFontManager
    {
    public:
        static constexpr spClassID ClassID = 0x31650C4A;

        spPS2FontManager() noexcept = default;
        ~spPS2FontManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
