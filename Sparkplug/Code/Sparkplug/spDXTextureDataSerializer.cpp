#include "spDXTextureDataSerializer.h"

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXTextureDataSerializer()
        {
            return std::make_unique<spDXTextureDataSerializer>();
        }

        const spRTTIRecord DXTextureDataSerializerRecord{
            spDXTextureDataSerializer::ClassID,
            spTextureDataSerializer::ClassID,
            "spDXTextureDataSerializer",
            &spTextureDataSerializer::StaticRTTI(),
            &CreateDXTextureDataSerializer,
            nullptr,
        };

        const bool DXTextureDataSerializerRegistered =
            spRTTIManager::Instance().Register(DXTextureDataSerializerRecord);
    }

    spDXTextureDataSerializer::~spDXTextureDataSerializer() = default;

    const spRTTIRecord& spDXTextureDataSerializer::StaticRTTI() noexcept
    {
        (void)DXTextureDataSerializerRegistered;
        return DXTextureDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spDXTextureDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXTextureDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXTextureDataSerializer::vfunc_18() const noexcept
    {
        return DXTextureDataSerializerRecord;
    }

    spClassID spDXTextureDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spTextureDataSerializer::Field>
    spDXTextureDataSerializer::BuildKnownWritePlanForAnalysis(
        const DataSourceKind sourceKind,
        const std::uint32_t nativeSerializationMode) const
    {
        switch (sourceKind)
        {
        case DataSourceKind::EmbeddedMemoryStream:
            return {Field::SourceEmbeded};
        case DataSourceKind::ReferencedStream:
            return {Field::SourceReference};
        case DataSourceKind::None:
            break;
        }

        std::vector<Field> plan{Field::SourceNone, Field::PlatformType};
        if (nativeSerializationMode == 0 || nativeSerializationMode == 2)
        {
            plan.push_back(Field::CrossPlatform);
        }
        plan.push_back(Field::PlatformSpecific);
        return plan;
    }

    std::uint32_t spDXTextureDataSerializer::PlatformTypeForAnalysis(
        const std::uint32_t nativeSerializationMode) noexcept
    {
        return nativeSerializationMode == 0 || nativeSerializationMode == 2
            ? DXPlatformAndCrossPlatformType
            : DXPlatformType;
    }

    bool spDXTextureDataSerializer::PCLoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PCNativeLoadFlagMask) != 0;
    }

    bool spDXTextureDataSerializer::PS2LoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PS2NativeLoadFlagMask) != 0;
    }

    spDXTextureDataSerializer::NativePayloadHeader
    spDXTextureDataSerializer::DescribeNativePayloadForAnalysis(
        const bool hasPlatformSpecificData,
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint32_t pixelFormat,
        const bool hasPixelData,
        const std::uint32_t mipCount) noexcept
    {
        return {
            mipCount != 0,
            hasPlatformSpecificData,
            width,
            height,
            pixelFormat,
            hasPixelData,
            mipCount,
        };
    }

    spDXTextureDataSerializer::NativeMipHeader
    spDXTextureDataSerializer::BuildNativeMipHeaderForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint32_t rowStride) noexcept
    {
        const auto payloadSize = static_cast<std::uint64_t>(rowStride) * height;
        if (payloadSize > std::numeric_limits<std::uint32_t>::max())
        {
            return {};
        }

        return {
            true,
            width,
            height,
            rowStride,
            static_cast<std::uint32_t>(payloadSize),
        };
    }
}
