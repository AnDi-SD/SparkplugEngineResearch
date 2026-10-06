#pragma once

// Original PC serializer identity. Physical and registered parent are the
// DX texture serializer; methods below recover its cube-specific overrides.
#include "spDXTextureSerializer.h"

namespace sparkplug::reconstruction
{
    class spDXCubeTextureSerializer final : public spDXTextureSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x269C2481;
        spDXCubeTextureSerializer() noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,
            spStream&, std::uint32_t, spBaseObject&, std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&, spBaseObject&) const override;
    };
}
