#pragma once

// The class name and diagnostics survive in both executables, but an exact
// original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spFogSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x576A70CA;
        static constexpr spClassID TargetClassID = 0x7AC95AEC;

        enum class Field : std::uint32_t
        {
            Fog = 0,
        };

        struct FogPayload final
        {
            std::uint32_t type = 0;
            std::uint32_t colorARGB = 0;
            float start = 0.0F;
            float end = 0.0F;
            float density = 0.0F;

            [[nodiscard]] bool operator==(const FogPayload& other) const noexcept;
        };

        spFogSerializer() noexcept = default;
        ~spFogSerializer() override;

        spFogSerializer(const spFogSerializer&) = delete;
        spFogSerializer& operator=(const spFogSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis();
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
