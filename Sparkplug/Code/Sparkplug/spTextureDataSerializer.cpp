#include "spTextureDataSerializer.h"

#include "spTextureData.h"

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextureDataSerializer()
        {
            return std::make_unique<spTextureDataSerializer>();
        }

        const spRTTIRecord TextureDataSerializerRecord{
            spTextureDataSerializer::ClassID,
            spSerializer::ClassID,
            "spTextureDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateTextureDataSerializer,
            nullptr,
        };

        const bool TextureDataSerializerRegistered =
            spRTTIManager::Instance().Register(TextureDataSerializerRecord);
    }

    spTextureDataSerializer::~spTextureDataSerializer() = default;

    const spRTTIRecord& spTextureDataSerializer::StaticRTTI() noexcept
    {
        (void)TextureDataSerializerRegistered;
        return TextureDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTextureDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTextureDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTextureDataSerializer::vfunc_18() const noexcept
    {
        return TextureDataSerializerRecord;
    }

    spClassID spTextureDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spTextureData::ClassID;
    }

    std::vector<spTextureDataSerializer::Field>
    spTextureDataSerializer::BuildKnownWritePlanForAnalysis(
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

        std::vector<Field> plan{Field::SourceNone};
        if (nativeSerializationMode == 0 || nativeSerializationMode == 2)
        {
            plan.push_back(Field::PlatformType);
            plan.push_back(Field::CrossPlatform);
        }
        return plan;
    }

    spTextureDataSerializer::CrossPlatformPayloadHeader
    spTextureDataSerializer::BuildCrossPlatformPayloadHeaderForAnalysis(
        const spTextureData& textureData) noexcept
    {
        CrossPlatformPayloadHeader result;
        const auto& buffer = textureData.GetTextureBufferForAnalysis();
        if (!buffer.IsInitializedForAnalysis())
        {
            return result;
        }

        result.width = buffer.GetWidthForAnalysis();
        result.height = buffer.GetHeightForAnalysis();
        result.pixelFormat = buffer.GetPixelFormatForAnalysis();
        result.pixelSize = buffer.GetPixelSizeForAnalysis();

        const std::uint64_t payloadSize =
            static_cast<std::uint64_t>(result.width)
            * result.height
            * result.pixelSize;
        if (payloadSize > std::numeric_limits<std::uint32_t>::max()
            || payloadSize > buffer.GetBufferForAnalysis().size())
        {
            return result;
        }

        result.payloadSize = static_cast<std::uint32_t>(payloadSize);
        result.valid = true;
        return result;
    }
}
