#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spDXTextureDataSerializer.cpp

#include "spTextureDataSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXTextureDataSerializer final : public spTextureDataSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x1C6D480F;
        static constexpr spClassID TargetClassID = 0x0B1C67BB;

        static constexpr std::uint32_t PCNativeLoadFlagMask = 0x00000002;
        static constexpr std::uint32_t PS2NativeLoadFlagMask = 0x00000008;
        static constexpr std::uint32_t DXPlatformType = 6;
        static constexpr std::uint32_t DXPlatformAndCrossPlatformType = 7;
        static constexpr std::uint32_t NativeMipRecordSize = 0x10;

        enum class NativeField : std::uint32_t
        {
            Data = 0,
            Mipmap = 1,
        };

        struct NativePayloadHeader final
        {
            bool valid = false;
            bool hasPlatformSpecificData = false;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t pixelFormat = 0;
            bool hasPixelData = false;
            std::uint32_t mipCount = 0;
        };

        struct NativeMipHeader final
        {
            bool valid = false;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t rowStride = 0;
            std::uint32_t payloadSize = 0;
        };

        spDXTextureDataSerializer() noexcept = default;
        ~spDXTextureDataSerializer() override;

        spDXTextureDataSerializer(const spDXTextureDataSerializer&) = delete;
        spDXTextureDataSerializer& operator=(
            const spDXTextureDataSerializer&) = delete;

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
        [[nodiscard]] static NativePayloadHeader DescribeNativePayloadForAnalysis(
            bool hasPlatformSpecificData,
            std::uint32_t width,
            std::uint32_t height,
            std::uint32_t pixelFormat,
            bool hasPixelData,
            std::uint32_t mipCount) noexcept;
        [[nodiscard]] static NativeMipHeader BuildNativeMipHeaderForAnalysis(
            std::uint32_t width,
            std::uint32_t height,
            std::uint32_t rowStride) noexcept;
    };
}
