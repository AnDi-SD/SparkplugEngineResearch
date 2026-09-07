// Inferred translation-unit path.  No separate spStream.cpp path survives in
// either executable; only the neighboring spMemoryStream.cpp path is exact.
// The split is retained for a small buildable reconstruction and is not
// presented as proof of the original file layout.

#include "spStream.h"

#include <cstring>
#include <limits>
#include <vector>

namespace sparkplug::reconstruction
{
    spStream::~spStream() = default;

    const spRTTIRecord& spStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spCrossPlatform::ClassID,
            "spStream",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };
        return record;
    }

    std::unique_ptr<spBaseObject> spStream::vfunc_10(spCloneManager&) const
    {
        // PS2 sub_00114F90 is a class-local null stub; PC shares the equivalent
        // root null-clone target.  Streams are not cloned through this slot.
        return nullptr;
    }

    const spRTTIRecord& spStream::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    void* spStream::GetBuffer() noexcept
    {
        // PS2 sub_00114E50 and PC 0x004A1BF0 return null.
        return nullptr;
    }

    const char* spStream::GetStreamName() const noexcept
    {
        return streamName_.has_value() ? streamName_->c_str() : nullptr;
    }

    bool spStream::Write(const char* value, const bool writeLength)
    {
        if (value == nullptr)
        {
            if (!writeLength)
            {
                return true;
            }

            const std::uint16_t length = 0;
            return WriteData(&length, sizeof(length));
        }

        const std::size_t sourceLength = std::strlen(value);
        if (sourceLength >= std::numeric_limits<std::uint16_t>::max())
        {
            // The native helper truncates through 16 bits.  Rejecting the
            // lossy case is an intentional safety difference in this facade.
            return false;
        }

        const auto lengthWithTerminator =
            static_cast<std::uint16_t>(sourceLength + 1);
        if (writeLength
            && !WriteData(&lengthWithTerminator, sizeof(lengthWithTerminator)))
        {
            return false;
        }

        const auto bytesToWrite = writeLength
            ? static_cast<std::uint32_t>(lengthWithTerminator)
            : static_cast<std::uint32_t>(sourceLength);
        return WriteData(value, bytesToWrite);
    }

    bool spStream::ReadString(std::string& value, bool* wasNull)
    {
        std::uint16_t byteCount = 0;
        if (!ReadData(&byteCount, sizeof(byteCount)))
        {
            return false;
        }

        if (byteCount == 0)
        {
            value.clear();
            if (wasNull) *wasNull = true;
            return true;
        }

        std::vector<char> buffer(byteCount);
        if (!ReadData(buffer.data(), byteCount))
        {
            return false;
        }

        if (buffer.back() == '\0')
        {
            value.assign(buffer.data(), buffer.size() - 1);
        }
        else
        {
            // Malformed native data need not be zero-terminated.  Avoid an
            // out-of-bounds C-string scan while retaining all bytes.
            value.assign(buffer.data(), buffer.size());
        }
        if (wasNull) *wasNull = false;
        return true;
    }

    bool spStream::CopyTo(spStream& destination)
    {
        std::uint32_t size = 0;
        if (!GetSize(&size))
        {
            return false;
        }

        (void)destination.vfunc_WriteFromStream(this, size);
        return true;
    }

    bool spStream::SetStreamName(const char* streamName)
    {
        if (streamName == nullptr)
        {
            streamName_.reset();
            return true;
        }
        streamName_ = streamName;
        return true;
    }

    std::uint32_t spStream::GetLogicalOrigin() const noexcept
    {
        return logicalOrigin_;
    }

    void spStream::SetLogicalOrigin(const std::uint32_t origin) noexcept
    {
        logicalOrigin_ = origin;
    }
}
