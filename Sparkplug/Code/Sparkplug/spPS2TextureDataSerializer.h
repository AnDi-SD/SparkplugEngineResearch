#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spPS2TextureDataSerializer.cpp

#include "spTextureDataSerializer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPS2TextureDataSerializer final : public spTextureDataSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x43C76799;
        static constexpr spClassID TargetClassID = 0x24767C83;

        static constexpr std::uint32_t PCNativeLoadFlagMask = 0x00000002;
        static constexpr std::uint32_t PS2NativeLoadFlagMask = 0x00000008;
        static constexpr std::uint32_t PS2PlatformType = 8;
        static constexpr std::uint32_t PS2PlatformAndCrossPlatformType = 9;
        static constexpr std::uint32_t NativeMipRecordSize = 0x14;
        static constexpr std::uint32_t NativeMipHeaderWordCount = 4;

        struct NativePayloadHeader final
        {
            bool hasPlatformSpecificData = false;
            std::uint32_t pixelFormat = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t auxiliaryValue = 0;
            std::uint32_t mipCount = 0;
            std::uint32_t paletteByteCount = 0;
        };

        // Host observation of PS2 native section176A10, not a PS2 runtime
        // target, attachment, pixel decoder or GPU operation. All offsets use
        // the input stream's logical coordinate system. Limits are host guards.
        static constexpr std::uint32_t MaximumInspectionBytes = 16u * 1024u * 1024u;
        static constexpr std::uint32_t MaximumInspectionFields = 65536;
        static constexpr std::uint32_t MaximumInspectionImages = 4096;
        static constexpr std::uint32_t MaximumInspectionMips = 65536;
        static constexpr std::uint32_t NoImageForAnalysis = 0xffffffffu;

        struct NativeMipInspectionForAnalysis final
        {
            std::uint32_t descriptor0 = 0, descriptor1 = 0, descriptor2 = 0;
            std::uint32_t dataSize = 0, descriptorOffset = 0, dataOffset = 0;
        };

        struct NativeImageInspectionForAnalysis final
        {
            std::uint32_t fieldIndex = 0;
            std::uint8_t nativeFlag = 0; // Raw byte, not a presence gate.
            std::uint32_t pixelFormat = 0, width = 0, height = 0;
            std::uint32_t auxiliaryValue = 0, mipCount = 0;
            std::uint32_t paletteOffset = 0, paletteByteCount = 0;
            std::vector<NativeMipInspectionForAnalysis> mips;
            bool complete = false;
        };

        struct NativeFieldInspectionForAnalysis final
        {
            std::uint32_t fieldID = 0, headerOffset = 0, payloadOffset = 0, payloadSize = 0;
            std::uint32_t imageIndex = NoImageForAnalysis;
            bool terminator = false, complete = false;
        };

        struct NativeSectionInspectionForAnalysis final
        {
            std::uint32_t inputOffset = 0, inputSize = 0, finalPosition = 0;
            std::vector<NativeFieldInspectionForAnalysis> fields;
            std::vector<NativeImageInspectionForAnalysis> images;
            bool complete = false;
        };

        // Repeated field0 images and unknown fields remain ordered observations.
        // Completion requires a terminator at the exact supplied section end.
        // On failure completed observations are retained, complete stays false,
        // and finalPosition is the observed cursor; there is no rollback claim.
        [[nodiscard]] static bool InspectNativeSectionForAnalysis(
            spStream&, std::uint32_t size, NativeSectionInspectionForAnalysis&,
            std::string* error = nullptr);

        spPS2TextureDataSerializer() noexcept = default;
        ~spPS2TextureDataSerializer() override;

        spPS2TextureDataSerializer(const spPS2TextureDataSerializer&) = delete;
        spPS2TextureDataSerializer& operator=(
            const spPS2TextureDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            DataSourceKind sourceKind,
            std::uint32_t nativeSerializationMode) const;
        [[nodiscard]] static std::uint32_t PlatformTypeForAnalysis(
            std::uint32_t nativeSerializationMode) noexcept;
        [[nodiscard]] static bool PCLoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
        [[nodiscard]] static bool PS2LoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
        [[nodiscard]] static std::uint32_t PaletteByteCountForAnalysis(
            std::uint32_t pixelFormat) noexcept;
        [[nodiscard]] static NativePayloadHeader DescribeNativePayloadForAnalysis(
            bool hasPlatformSpecificData,
            std::uint32_t pixelFormat,
            std::uint32_t width,
            std::uint32_t height,
            std::uint32_t auxiliaryValue,
            std::uint32_t mipCount) noexcept;
    };
}
