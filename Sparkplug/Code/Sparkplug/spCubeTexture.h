#pragma once

// Original class identity; source path inferred. PC006F1F50 and PS20048EAD0
// retain the abstract spITexture boundary and add no storage to spTexture.
#include "spTexture.h"

namespace sparkplug::reconstruction
{
    class spCubeTexture : public spTexture
    {
    public:
        static constexpr spClassID ClassID = 0x65557907;
        ~spCubeTexture() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

    protected:
        spCubeTexture() noexcept = default;
    };
}
