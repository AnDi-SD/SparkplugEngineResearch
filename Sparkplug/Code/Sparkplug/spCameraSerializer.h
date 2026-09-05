#pragma once

// The class and field diagnostics survive in both executables, but an exact
// original translation-unit path has not yet been recovered.

#include "spNodeSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spCameraSerializer : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x440E53FB;
        static constexpr spClassID TargetClassID = 0x18DF3845;

        enum class Field : std::uint32_t
        {
            Camera = 0,
            TwoDimensional = 1,
        };

        struct CameraPayload final
        {
            float nearClipPlane = 0.0F;
            float farClipPlane = 0.0F;
            float viewAngle = 0.0F;
            float pixelAspectRatio = 0.0F;
        };

        spCameraSerializer() noexcept = default;
        ~spCameraSerializer() override;

        spCameraSerializer(const spCameraSerializer&) = delete;
        spCameraSerializer& operator=(const spCameraSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis(
            bool isTwoDimensional);
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
