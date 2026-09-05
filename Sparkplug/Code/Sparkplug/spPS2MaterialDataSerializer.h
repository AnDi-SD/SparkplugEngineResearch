#pragma once

// Both executables place this class beside the common material serializers,
// but neither one preserves an exact original header/translation-unit path.

#include "spMaterialSerializer.h"

namespace sparkplug::reconstruction
{
    class spPS2MaterialDataSerializer : public spMaterialSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x69327633;

        spPS2MaterialDataSerializer() noexcept = default;
        ~spPS2MaterialDataSerializer() override;

        spPS2MaterialDataSerializer(const spPS2MaterialDataSerializer&) = delete;
        spPS2MaterialDataSerializer& operator=(
            const spPS2MaterialDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
