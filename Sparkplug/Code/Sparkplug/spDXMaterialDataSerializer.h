#pragma once

// Both executables place this class beside the common material serializers,
// but neither one preserves an exact original header/translation-unit path.

#include "spMaterialSerializer.h"

namespace sparkplug::reconstruction
{
    class spDXMaterialDataSerializer : public spMaterialSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x60EE3A89;

        spDXMaterialDataSerializer() noexcept = default;
        ~spDXMaterialDataSerializer() override;

        spDXMaterialDataSerializer(const spDXMaterialDataSerializer&) = delete;
        spDXMaterialDataSerializer& operator=(
            const spDXMaterialDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
