#pragma once
// Original PC identity; path inferred from its DX runtime target.
#include "../Sparkplug/spMaterialSerializer.h"
namespace sparkplug::reconstruction
{
    class spDXMaterialSerializer final : public spMaterialSerializer
    {
    public:
        static constexpr spClassID ClassID=0x177E2F26;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Primary header hook is common467550, NOT data hook42F4C0.
        // DX-specific layer helpers4B0EB0/4B12A0/4B0F10 remain to be closed.
    };
}
