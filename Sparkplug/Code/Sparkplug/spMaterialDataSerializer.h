#pragma once

// The class is present in both executables, but no original translation-unit
// path was recovered. Placement under Code/Sparkplug follows its base class.

#include "spMaterialSerializer.h"

namespace sparkplug::reconstruction
{
    class spMaterialDataSerializer : public spMaterialSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x0B251467;
        static constexpr spClassID TargetClassID = 0x6160348B;

        spMaterialDataSerializer() noexcept = default;
        ~spMaterialDataSerializer() override;

        spMaterialDataSerializer(const spMaterialDataSerializer&) = delete;
        spMaterialDataSerializer& operator=(
            const spMaterialDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] static bool CanReadIntoObjectForAnalysis(
            bool hasTargetObject) noexcept;
    };
}
