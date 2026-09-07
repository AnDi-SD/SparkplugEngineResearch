#include "spPS2TextureDataSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2TextureDataSerializer()
        {
            return std::make_unique<spPS2TextureDataSerializer>();
        }

        const spRTTIRecord PS2TextureDataSerializerRecord{
            spPS2TextureDataSerializer::ClassID,
            spTextureDataSerializer::ClassID,
            "spPS2TextureDataSerializer",
            &spTextureDataSerializer::StaticRTTI(),
            &CreatePS2TextureDataSerializer,
            nullptr,
        };

        const bool PS2TextureDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2TextureDataSerializerRecord);
    }

    spPS2TextureDataSerializer::~spPS2TextureDataSerializer() = default;

    const spRTTIRecord& spPS2TextureDataSerializer::StaticRTTI() noexcept
    {
        (void)PS2TextureDataSerializerRegistered;
        return PS2TextureDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2TextureDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2TextureDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2TextureDataSerializer::vfunc_18() const noexcept
    {
        return PS2TextureDataSerializerRecord;
    }

    spClassID spPS2TextureDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spTextureDataSerializer::Field>
    spPS2TextureDataSerializer::BuildKnownWritePlanForAnalysis(
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

    std::uint32_t spPS2TextureDataSerializer::PlatformTypeForAnalysis(
        const std::uint32_t nativeSerializationMode) noexcept
    {
        return nativeSerializationMode == 0 || nativeSerializationMode == 2
            ? PS2PlatformAndCrossPlatformType
            : PS2PlatformType;
    }

    bool spPS2TextureDataSerializer::PCLoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PCNativeLoadFlagMask) != 0;
    }

    bool spPS2TextureDataSerializer::PS2LoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PS2NativeLoadFlagMask) != 0;
    }

    std::uint32_t spPS2TextureDataSerializer::PaletteByteCountForAnalysis(
        const std::uint32_t pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case 0:
            return 0x40;
        case 1:
            return 0x400;
        default:
            return 0;
        }
    }

    spPS2TextureDataSerializer::NativePayloadHeader
    spPS2TextureDataSerializer::DescribeNativePayloadForAnalysis(
        const bool hasPlatformSpecificData,
        const std::uint32_t pixelFormat,
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint32_t auxiliaryValue,
        const std::uint32_t mipCount) noexcept
    {
        return {
            hasPlatformSpecificData,
            pixelFormat,
            width,
            height,
            auxiliaryValue,
            mipCount,
            PaletteByteCountForAnalysis(pixelFormat),
        };
    }
}
