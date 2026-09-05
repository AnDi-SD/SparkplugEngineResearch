// Exact original PC source path recovered from assertion metadata:
//   Z:\Sparkplug\Code\SparkBase\spMemoryStream.cpp

#include "spMemoryStream.h"

#include <cstring>
#include <limits>
#include <new>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMemoryStream()
        {
            return std::make_unique<spMemoryStream>();
        }
    }

    spMemoryStream::~spMemoryStream()
    {
        // Both native deleting destructors dispatch Close before destroying
        // the spStream base.
        (void)Close();
    }

    const spRTTIRecord& spMemoryStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spStream::ClassID,
            "spMemoryStream",
            &spStream::StaticRTTI(),
            &CreateMemoryStream,
            nullptr,
        };
        return record;
    }

    std::unique_ptr<spBaseObject> spMemoryStream::vfunc_10(
        spCloneManager& manager) const
    {
        // Native clone construction creates a fresh default stream and then
        // invokes inherited slot +0x14.  Buffer, counters and diagnostic
        // stream name are consequently not copied; spNamedObject state is.
        auto clone = std::make_unique<spMemoryStream>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spMemoryStream::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    bool spMemoryStream::Open(const char* streamName)
    {
        (void)SetStreamName(streamName);

        // Native Open does not close an existing buffer and does not check the
        // allocation result.  Overwriting an open stream therefore leaks in
        // the original; preserve its visible state transition without making
        // allocation failure fatal.
        buffer_ = new (std::nothrow) std::uint8_t[growthQuantum_];
        capacity_ = growthQuantum_;
        size_ = 0;
        position_ = 0;
        return true;
    }

    bool spMemoryStream::Open(std::uint32_t, const char* streamName)
    {
        // Both builds ignore the mode and tail-call the one-argument slot.
        return Open(streamName);
    }

    bool spMemoryStream::Close()
    {
        if (ownsBuffer_)
        {
            delete[] buffer_;
        }
        buffer_ = nullptr;

        // Native Close intentionally leaves capacity, size, position, flags
        // and the diagnostic name untouched.
        return true;
    }

    bool spMemoryStream::Seek(const SeekSource source, const std::int32_t offset)
    {
        std::int64_t target = 0;
        switch (source)
        {
        case SeekSource::essStart:
            target = static_cast<std::int64_t>(GetLogicalOrigin()) + offset;
            break;
        case SeekSource::essEnd:
            // This surprising capacity-relative expression is identical in
            // the PC and PS2 code.  It is not normalized to logical size.
            target = static_cast<std::int64_t>(capacity_) - offset - 1;
            break;
        case SeekSource::essCurrent:
            target = static_cast<std::int64_t>(position_) + offset;
            break;
        default:
            target = 0;
            break;
        }

        if (target < 0 || target > static_cast<std::int64_t>(size_))
        {
            return false;
        }

        // The native fast path never checks buffer_, so a closed empty stream
        // accepts Seek(essStart, 0).
        position_ = static_cast<std::uint32_t>(target);
        return true;
    }

    bool spMemoryStream::GetCurrentPosition(std::uint32_t& position) const
    {
        if (buffer_ == nullptr)
        {
            return false;
        }
        position = position_ - GetLogicalOrigin();
        return true;
    }

    bool spMemoryStream::ReadData(
        void* destination,
        const std::uint32_t byteCount)
    {
        if (byteCount > std::numeric_limits<std::uint32_t>::max() - position_)
        {
            return false;
        }

        const std::uint32_t end = position_ + byteCount;
        if (end > size_)
        {
            return false;
        }

        if (byteCount != 0)
        {
            // Native code would dereference invalid caller pointers.  Failing
            // safely is the only intentional difference on this path.
            if (buffer_ == nullptr || destination == nullptr)
            {
                return false;
            }
            std::memcpy(destination, buffer_ + position_, byteCount);
        }
        position_ = end;
        return true;
    }

    bool spMemoryStream::PrepareWrite(const std::uint32_t byteCount)
    {
        if (buffer_ == nullptr
            || byteCount > std::numeric_limits<std::uint32_t>::max() - position_)
        {
            return false;
        }

        const std::uint32_t end = position_ + byteCount;
        if (end < size_)
        {
            return true;
        }
        if (end < capacity_)
        {
            size_ = end;
            return true;
        }
        if (!resizeEnabled_)
        {
            return false;
        }

        std::uint32_t newCapacity = capacity_;
        while (newCapacity < end)
        {
            if (growthQuantum_ == 0
                || growthQuantum_
                    > std::numeric_limits<std::uint32_t>::max() - newCapacity)
            {
                return false;
            }
            newCapacity += growthQuantum_;
        }

        auto* replacement = new (std::nothrow) std::uint8_t[newCapacity];
        if (replacement == nullptr && newCapacity != 0)
        {
            return false;
        }
        if (size_ != 0)
        {
            std::memcpy(replacement, buffer_, size_);
        }

        // The native growth helper frees the old buffer unconditionally; the
        // ownership flag controls Close only.
        delete[] buffer_;
        buffer_ = replacement;
        capacity_ = newCapacity;
        size_ = end;
        return true;
    }

    bool spMemoryStream::WriteData(
        const void* source,
        const std::uint32_t byteCount)
    {
        if ((byteCount != 0 && source == nullptr) || !PrepareWrite(byteCount))
        {
            return false;
        }
        if (byteCount != 0)
        {
            std::memcpy(buffer_ + position_, source, byteCount);
        }
        position_ += byteCount;
        return true;
    }

    bool spMemoryStream::vfunc_WriteFromStream(
        spStream* source,
        const std::uint32_t byteCount)
    {
        if (source == nullptr || !PrepareWrite(byteCount))
        {
            return false;
        }
        if (!source->ReadData(buffer_ + position_, byteCount))
        {
            // Native preflight may already have enlarged logical size even
            // though the source read fails; only position stays unchanged.
            return false;
        }
        position_ += byteCount;
        return true;
    }

    bool spMemoryStream::GetSize(std::uint32_t* size) const
    {
        if (buffer_ == nullptr || size == nullptr)
        {
            return false;
        }
        *size = size_;
        return true;
    }

    void* spMemoryStream::GetBuffer() noexcept
    {
        return buffer_;
    }

    bool spMemoryStream::ResizeAndSetSize(const std::uint32_t byteCount)
    {
        // PC 0x00465500 is protected at its entry, but twelve callers expose
        // this exact contract; PS2 0x00112260 is fully readable.
        delete[] buffer_;
        buffer_ = new (std::nothrow) std::uint8_t[byteCount];
        position_ = 0;
        size_ = 0;
        capacity_ = byteCount;
        size_ = byteCount;
        return true;
    }

    std::uint8_t* spMemoryStream::ReleaseBuffer() noexcept
    {
        ownsBuffer_ = false;
        resizeEnabled_ = false;
        return buffer_;
    }

    bool spMemoryStream::Reset() noexcept
    {
        size_ = 0;
        position_ = 0;
        return true;
    }

    std::uint32_t spMemoryStream::GetCapacity() const noexcept
    {
        return capacity_;
    }

    bool spMemoryStream::IsResizeEnabled() const noexcept
    {
        return resizeEnabled_;
    }

    bool spMemoryStream::OwnsBuffer() const noexcept
    {
        return ownsBuffer_;
    }
}
