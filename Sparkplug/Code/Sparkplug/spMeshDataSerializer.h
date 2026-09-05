#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spMeshDataSerializer.cpp
// The PS2 executable independently preserves "spMeshDataSerializer.cpp".

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMeshDataSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x66380037;
        static constexpr spClassID TargetClassID = 0x33C34CF0;

        enum class Field : std::uint32_t
        {
            CrossPlatform = 0,
        };

        spMeshDataSerializer() noexcept = default;
        ~spMeshDataSerializer() override;

        spMeshDataSerializer(const spMeshDataSerializer&) = delete;
        spMeshDataSerializer& operator=(const spMeshDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;

        // The original selector type/name is not yet known. PS2 proves that
        // native values 0 and 2 emit field 0; all other values omit it.
        [[nodiscard]] static bool EmitsCrossPlatformPayloadForAnalysis(
            std::uint32_t nativeSerializationMode) noexcept;
        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            std::uint32_t nativeSerializationMode) const;
    };
}
