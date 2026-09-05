// Exact original PC source path:
//   Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp

#include "spDataBlockSerializer.h"

#include <limits>

namespace sparkplug::reconstruction
{
    const spDataBlockHeaderForAnalysis*
    spDataBlockSerializer::ReadHeaderForAnalysis(spStream& source) noexcept
    {
        currentHeader_ = {};
        if (!source.GetCurrentPosition(currentHeader_.headerStreamPosition))
        {
            return nullptr;
        }

        std::uint8_t encoded = 0;
        if (!source.Read(encoded))
        {
            return nullptr;
        }

        currentHeader_.fieldID = encoded & 0x1FU;
        const auto sizeCode = static_cast<SizeCode>(encoded >> 5U);
        if (currentHeader_.fieldID == InlineFieldIDLimit)
        {
            std::uint8_t extendedID = 0;
            if (!source.Read(extendedID))
            {
                return nullptr;
            }
            currentHeader_.fieldID = extendedID;
        }

        switch (sizeCode)
        {
        case SizeCode::Empty:
            currentHeader_.fieldID =
                spDataBlockHeaderForAnalysis::InvalidFieldID;
            currentHeader_.payloadSize = 0;
            break;
        case SizeCode::Fixed1:
            currentHeader_.payloadSize = 1;
            break;
        case SizeCode::Fixed2:
            currentHeader_.payloadSize = 2;
            break;
        case SizeCode::Fixed4:
            currentHeader_.payloadSize = 4;
            break;
        case SizeCode::Fixed8:
            currentHeader_.payloadSize = 8;
            break;
        case SizeCode::UInt8:
        {
            std::uint8_t size = 0;
            if (!source.Read(size))
            {
                return nullptr;
            }
            currentHeader_.payloadSize = size;
            break;
        }
        case SizeCode::UInt16:
        {
            std::uint16_t size = 0;
            if (!source.Read(size))
            {
                return nullptr;
            }
            currentHeader_.payloadSize = size;
            break;
        }
        case SizeCode::UInt32:
            if (!source.Read(currentHeader_.payloadSize))
            {
                return nullptr;
            }
            break;
        }

        if (!source.GetCurrentPosition(currentHeader_.dataStreamPosition))
        {
            return nullptr;
        }
        return &currentHeader_;
    }

    bool spDataBlockSerializer::SkipDataForAnalysis(
        spStream& source,
        const spDataBlockHeaderForAnalysis& header) noexcept
    {
        if (header.payloadSize
            > std::numeric_limits<std::uint32_t>::max()
                - header.dataStreamPosition)
        {
            return false;
        }
        const std::uint32_t end =
            header.dataStreamPosition + header.payloadSize;
        if (end > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            return false;
        }
        return source.Seek(
            spStream::SeekSource::essStart,
            static_cast<std::int32_t>(end));
    }

    bool spDataBlockSerializer::WriteFieldForAnalysis(
        spStream& destination,
        const std::uint32_t fieldID,
        const void* const payload,
        const std::uint32_t payloadSize) const noexcept
    {
        if ((payloadSize != 0 && payload == nullptr)
            || !WriteHeaderForAnalysis(destination, fieldID, payloadSize))
        {
            return false;
        }
        return destination.WriteData(payload, payloadSize);
    }

    bool spDataBlockSerializer::WriteTerminatorForAnalysis(
        spStream& destination) noexcept
    {
        const std::uint8_t terminator = 0;
        return destination.Write(terminator);
    }

    spDataBlockSerializer::SizeCode
    spDataBlockSerializer::SelectSizeCodeForAnalysis(
        const std::uint32_t payloadSize) noexcept
    {
        switch (payloadSize)
        {
        case 1:
            return SizeCode::Fixed1;
        case 2:
            return SizeCode::Fixed2;
        case 4:
            return SizeCode::Fixed4;
        case 8:
            return SizeCode::Fixed8;
        default:
            if (payloadSize <= std::numeric_limits<std::uint8_t>::max())
            {
                return SizeCode::UInt8;
            }
            if (payloadSize <= std::numeric_limits<std::uint16_t>::max())
            {
                return SizeCode::UInt16;
            }
            return SizeCode::UInt32;
        }
    }

    const spDataBlockHeaderForAnalysis&
    spDataBlockSerializer::GetCurrentHeaderForAnalysis() const noexcept
    {
        return currentHeader_;
    }

    bool spDataBlockSerializer::WriteHeaderForAnalysis(
        spStream& destination,
        const std::uint32_t fieldID,
        const std::uint32_t payloadSize) noexcept
    {
        if (fieldID > ExtendedFieldIDLimit)
        {
            return false;
        }

        const SizeCode sizeCode = SelectSizeCodeForAnalysis(payloadSize);
        const auto sizeBits = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(sizeCode) << 5U);
        const auto inlineID = static_cast<std::uint8_t>(
            fieldID < InlineFieldIDLimit ? fieldID : InlineFieldIDLimit);
        const std::uint8_t encoded = sizeBits | inlineID;
        if (!destination.Write(encoded))
        {
            return false;
        }
        if (fieldID >= InlineFieldIDLimit)
        {
            const auto extendedID = static_cast<std::uint8_t>(fieldID);
            if (!destination.Write(extendedID))
            {
                return false;
            }
        }

        switch (sizeCode)
        {
        case SizeCode::UInt8:
            return destination.Write(static_cast<std::uint8_t>(payloadSize));
        case SizeCode::UInt16:
            return destination.Write(static_cast<std::uint16_t>(payloadSize));
        case SizeCode::UInt32:
            return destination.Write(payloadSize);
        default:
            return true;
        }
    }
}
