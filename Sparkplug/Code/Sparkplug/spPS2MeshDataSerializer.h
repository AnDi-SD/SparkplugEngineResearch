#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spPS2MeshDataSerializer.cpp
// The PS2 executable independently preserves "spPS2MeshDataSerializer.cpp".

#include "spMeshDataSerializer.h"

#include <cstdint>
#include <array>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPS2MeshDataSerializer final : public spMeshDataSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x6B0C238F;
        static constexpr spClassID TargetClassID = 0x737D740F;

        // The spelling of field 0 deliberately follows the native diagnostic.
        enum class Field : std::uint32_t
        {
            CrossPlatform = 0,
            PlatformSpecific = 1,
            BoundingBox = 2,
        };

        // The two executables use different bits in the reader configuration
        // word to choose the native packet branch. Keep that ABI difference
        // visible instead of folding it into a guessed common enum.
        static constexpr std::uint32_t PCNativeLoadFlagMask = 0x00000002;
        static constexpr std::uint32_t PS2NativeLoadFlagMask = 0x00000008;

        // Host snapshots of the actual serialized prefix/field, not RTTI game
        // objects or executable PS2 packets. PC42A420/42AC2A, PS2163160/1635C8.
        struct NativePayloadHeaderForAnalysis {
            std::array<float,4> sphere{};
            std::uint32_t primitiveCount=0,vertexCount=0,componentFlags=0,
                packetQwords=0,additionalUVCount=0,weightCount=0;
        };
        struct BoundingBoxForAnalysis {std::array<float,3> minimum{},maximum{};};
        [[nodiscard]] static bool ReadNativeHeaderForAnalysis(spStream&,NativePayloadHeaderForAnalysis&);
        [[nodiscard]] static bool ReadBoundingBoxForAnalysis(spStream&,BoundingBoxForAnalysis&);

        spPS2MeshDataSerializer() noexcept = default;
        ~spPS2MeshDataSerializer() override;

        spPS2MeshDataSerializer(const spPS2MeshDataSerializer&) = delete;
        spPS2MeshDataSerializer& operator=(const spPS2MeshDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            std::uint32_t nativeSerializationMode) const;
        [[nodiscard]] static bool PCLoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
        [[nodiscard]] static bool PS2LoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
    };
}
