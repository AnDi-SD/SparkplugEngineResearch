#include "spPS2TextureDataSerializer.h"
#include "spDataBlockSerializer.h"

#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        // Borrowed host view: common header decoding must not read even its
        // size bytes beyond the supplied section. No wire codec lives here.
        class BoundedTextureInspectionStream final : public spStream
        {
        public:
            BoundedTextureInspectionStream(spStream& source, std::uint32_t begin,
                std::uint32_t end) : source_(source), begin_(begin), end_(end) {}
            bool Open(const char*) override { return false; }
            bool Open(std::uint32_t, const char*) override { return false; }
            bool Close() override { return false; }
            bool WriteData(const void*, std::uint32_t) override { return false; }
            bool vfunc_WriteFromStream(spStream*, std::uint32_t) override { return false; }
            bool GetSize(std::uint32_t* size) const override
            { if (!size) return false; *size = end_; return true; }
            bool GetCurrentPosition(std::uint32_t& position) const override
            { return source_.GetCurrentPosition(position) && position >= begin_ && position <= end_; }
            bool ReadData(void* destination, std::uint32_t count) override
            {
                std::uint32_t position = 0;
                return GetCurrentPosition(position) && count <= end_ - position
                    && source_.ReadData(destination, count);
            }
            bool Seek(SeekSource source, std::int32_t offset) override
            {
                std::uint32_t position = 0;
                if (!GetCurrentPosition(position)) return false;
                std::int64_t target = offset;
                if (source == SeekSource::essCurrent) target += position;
                else if (source == SeekSource::essEnd) target += end_;
                else if (source != SeekSource::essStart) return false;
                if (target < begin_ || target > end_) return false;
                // Difference is bounded by the <=16MiB view even at a large
                // logical source offset; no signed absolute-seek truncation.
                return source_.Seek(SeekSource::essCurrent,
                    static_cast<std::int32_t>(target - position));
            }
        private:
            spStream& source_;
            std::uint32_t begin_, end_;
        };

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

    bool spPS2TextureDataSerializer::InspectNativeSectionForAnalysis(spStream& source,
        std::uint32_t size, NativeSectionInspectionForAnalysis& output, std::string* error)
    {
        if (error) error->clear();
        output = {};
        output.inputSize = size;
        const auto fail = [&](const char* message)
        {
            (void)source.GetCurrentPosition(output.finalPosition);
            if (error) *error = message;
            return false;
        };
        std::uint32_t physicalSize = 0;
        if (!source.GetCurrentPosition(output.inputOffset))
            return fail("Cannot locate PS2 texture metadata input");
        output.finalPosition = output.inputOffset;
        if (!size || size > MaximumInspectionBytes
            || size > std::numeric_limits<std::uint32_t>::max() - output.inputOffset)
            return fail("PS2 texture metadata section exceeds host size or UInt32 extent limit");
        if (!source.GetSize(&physicalSize) || source.GetLogicalOriginForAnalysis() > physicalSize)
            return fail("Cannot determine PS2 texture metadata source extent");
        const auto logicalSize = physicalSize - source.GetLogicalOriginForAnalysis();
        if (output.inputOffset > logicalSize || size > logicalSize - output.inputOffset)
            return fail("PS2 texture metadata section exceeds source extent");
        const auto end = output.inputOffset + size;
        BoundedTextureInspectionStream stream(source, output.inputOffset, end);
        spDataBlockSerializer blocks;
        std::uint32_t storedMips = 0;
        try
        {
            while (true)
            {
                std::uint32_t position = 0;
                if (!stream.GetCurrentPosition(position) || position >= end)
                    return fail("Missing PS2 texture metadata section terminator");
                if (output.fields.size() >= MaximumInspectionFields)
                    return fail("PS2 texture metadata field count exceeds host limit");
                const auto* header = blocks.ReadHeaderForAnalysis(stream); // PS2 17E890.
                if (!header) return fail("Truncated PS2 texture metadata field header");
                NativeFieldInspectionForAnalysis field;
                field.fieldID = header->fieldID;
                field.headerOffset = header->headerStreamPosition;
                field.payloadOffset = header->dataStreamPosition;
                field.payloadSize = header->payloadSize;
                field.terminator = header->IsTerminator();
                output.fields.push_back(field);
                auto& observedField = output.fields.back();
                if (field.payloadOffset > end || field.payloadSize > end - field.payloadOffset)
                    return fail("PS2 texture metadata field exceeds section extent");
                const auto fieldEnd = field.payloadOffset + field.payloadSize;
                if (field.terminator)
                {
                    observedField.complete = true;
                    output.finalPosition = field.payloadOffset;
                    if (field.payloadOffset != end)
                        return fail("Trailing bytes after PS2 texture metadata terminator");
                    output.complete = true;
                    return true;
                }
                if (field.fieldID != 0)
                {
                    // Original17E830 skips unknown data to payload+size.
                    if (!stream.Seek(spStream::SeekSource::essCurrent,
                            static_cast<std::int32_t>(field.payloadSize)))
                        return fail("Cannot skip unknown PS2 texture metadata field");
                    observedField.complete = true;
                    continue;
                }
                if (output.images.size() >= MaximumInspectionImages)
                    return fail("PS2 texture metadata image count exceeds host limit");
                observedField.imageIndex = static_cast<std::uint32_t>(output.images.size());
                output.images.emplace_back();
                auto& image = output.images.back();
                image.fieldIndex = static_cast<std::uint32_t>(output.fields.size() - 1);
                // Original176AB4..176AFC: byte always stored, then five words.
                // Host checks reject incomplete reads rather than using the
                // native reader's unchecked/uninitialized partial state.
                if (field.payloadSize < 21 || !stream.Read(image.nativeFlag)
                    || !stream.Read(image.pixelFormat) || !stream.Read(image.width)
                    || !stream.Read(image.height) || !stream.Read(image.auxiliaryValue)
                    || !stream.Read(image.mipCount))
                    return fail("Truncated PS2 texture metadata scalar header");
                image.paletteByteCount = PaletteByteCountForAnalysis(image.pixelFormat);
                if (!stream.GetCurrentPosition(image.paletteOffset)
                    || image.paletteOffset > fieldEnd
                    || image.paletteByteCount > fieldEnd - image.paletteOffset
                    || !stream.Seek(spStream::SeekSource::essCurrent,
                        static_cast<std::int32_t>(image.paletteByteCount)))
                    return fail("Truncated PS2 texture metadata palette");
                if (!stream.GetCurrentPosition(position) || position > fieldEnd)
                    return fail("Cannot locate PS2 texture metadata mip cursor");
                if (image.mipCount > MaximumInspectionMips - storedMips)
                    return fail("PS2 texture metadata mip count exceeds host limit");
                if (image.mipCount > (fieldEnd - position) / 16u)
                    return fail("PS2 texture metadata mip descriptors exceed field extent");
                image.mips.reserve(image.mipCount);
                for (std::uint32_t i = 0; i < image.mipCount; ++i)
                {
                    NativeMipInspectionForAnalysis mip;
                    if (!stream.GetCurrentPosition(mip.descriptorOffset)
                        || mip.descriptorOffset > fieldEnd || fieldEnd - mip.descriptorOffset < 16
                        || !stream.Read(mip.descriptor0) || !stream.Read(mip.descriptor1)
                        || !stream.Read(mip.descriptor2) || !stream.Read(mip.dataSize)
                        || !stream.GetCurrentPosition(mip.dataOffset))
                        return fail("Truncated PS2 texture metadata mip descriptor");
                    // Original176B78/176B94 use the fourth wire word directly.
                    // No guessed bpp, dimension or swizzle changes this cursor.
                    if (mip.dataOffset > fieldEnd || mip.dataSize > fieldEnd - mip.dataOffset
                        || !stream.Seek(spStream::SeekSource::essCurrent,
                            static_cast<std::int32_t>(mip.dataSize)))
                        return fail("PS2 texture metadata mip data exceeds field extent");
                    image.mips.push_back(mip);
                    ++storedMips;
                }
                if (!stream.GetCurrentPosition(position) || position != fieldEnd)
                    return fail("PS2 texture metadata image did not consume its exact field extent");
                image.complete = observedField.complete = true;
            }
        }
        catch (const std::bad_alloc&)
        { return fail("Cannot allocate bounded PS2 texture metadata observations"); }
    }

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
