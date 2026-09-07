// Exact original PC source path:
//   Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp

#include "spDataBlockSerializer.h"

#include <limits>
#include <new>

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
        return WriteHeaderWithCodeForAnalysis(
            destination, fieldID, payloadSize, SelectSizeCodeForAnalysis(payloadSize));
    }

    bool spDataBlockSerializer::WriteHeaderWithCodeForAnalysis(
        spStream& destination, const std::uint32_t fieldID,
        const std::uint32_t payloadSize, const SizeCode sizeCode) noexcept
    {
        if (fieldID > ExtendedFieldIDLimit)
        {
            return false;
        }

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

    bool spDataBlockSerializer::BeginObjectForAnalysis(
        spStream& destination, const spBaseObject* const object) noexcept
    {
        if (!writerHeaders_.empty())
            return false; // explicit host guard against retargeting an open stack
        writerObject_ = object;
        writerStream_ = &destination;
        return true;
    }

    bool spDataBlockSerializer::WriteBeginForAnalysis(
        const std::uint32_t fieldID, const SizeCode reservedSizeCode) noexcept
    {
        if (!writerStream_ || fieldID == InlineFieldIDLimit || fieldID > ExtendedFieldIDLimit || writerHeaders_.size() >= 64 ||
            (reservedSizeCode != SizeCode::UInt8 && reservedSizeCode != SizeCode::UInt16 &&
             reservedSizeCode != SizeCode::UInt32) ||
            (!writerHeaders_.empty() && reservedSizeCode_ != reservedSizeCode))
            return false;
        spDataBlockHeaderForAnalysis header{};
        header.fieldID = fieldID;
        header.payloadSize = 0xFFFFFFFFU;
        header.dataStreamPosition = 0xFFFFFFFFU;
        if (!writerStream_->GetCurrentPosition(header.headerStreamPosition) ||
            header.headerStreamPosition > std::uint32_t(std::numeric_limits<std::int32_t>::max()) - 6)
            return false;
        try
        {
            writerHeaders_.push_back(header);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        reservedSizeCode_ = reservedSizeCode;
        // Native reserves all-one size bytes, then patches in the same width.
        // Failure leaves a live header and a possibly partial output, not an
        // invented transaction rollback.
        if (!WriteHeaderWithCodeForAnalysis(*writerStream_, fieldID, 0xFFFFFFFFU, reservedSizeCode_))
            return false;
        return writerStream_->GetCurrentPosition(writerHeaders_.back().dataStreamPosition);
    }

    bool spDataBlockSerializer::WriteEndForAnalysis(const std::uint32_t ignoredFieldID) noexcept
    {
        (void)ignoredFieldID;
        if (!writerStream_ || writerHeaders_.empty())
            return false;
        const auto& header = writerHeaders_.back();
        std::uint32_t end = 0;
        if (!writerStream_->GetCurrentPosition(end) || end < header.dataStreamPosition ||
            end > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
            return false;
        const auto size = end - header.dataStreamPosition;
        if (size == 0)
            return false; // native reports success but leaves all-one placeholder
        if ((reservedSizeCode_ == SizeCode::UInt8 && size > 0xFFU) ||
            (reservedSizeCode_ == SizeCode::UInt16 && size > 0xFFFFU))
            return false; // native logs and continues; host refuses corrupt patch
        if (!writerStream_->Seek(spStream::SeekSource::essStart,
                                 static_cast<std::int32_t>(header.headerStreamPosition)) ||
            !WriteHeaderWithCodeForAnalysis(*writerStream_, header.fieldID, size, reservedSizeCode_) ||
            !writerStream_->Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(end)))
            return false;
        writerHeaders_.pop_back();
        return true;
    }

    bool spDataBlockSerializer::FinalizeObjectForAnalysis() noexcept
    {
        return writerStream_ && writerHeaders_.empty() && WriteTerminatorForAnalysis(*writerStream_);
    }
}
