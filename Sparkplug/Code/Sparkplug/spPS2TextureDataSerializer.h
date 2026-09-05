#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spPS2TextureDataSerializer.cpp

#include "spTextureDataSerializer.h"

#include <cstdint>
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
