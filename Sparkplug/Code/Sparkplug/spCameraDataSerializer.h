#pragma once

// Inferred placement beside spCameraSerializer. Neither executable preserves
// an exact original source path for this thin derived serializer.

#include "spCameraSerializer.h"

namespace sparkplug::reconstruction
{
    class spCameraDataSerializer : public spCameraSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x759F1687;

        // Both shipped vtables explicitly keep the spCamera target. The name
        // spCameraDataSerializer must not be used to infer another target ID.
        static constexpr spClassID TargetClassID =
            spCameraSerializer::TargetClassID;

        spCameraDataSerializer() noexcept = default;
        ~spCameraDataSerializer() override;

        spCameraDataSerializer(const spCameraDataSerializer&) = delete;
        spCameraDataSerializer& operator=(
            const spCameraDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
    };
}
