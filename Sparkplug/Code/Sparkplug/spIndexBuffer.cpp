#include "spIndexBuffer.h"

#include "../SparkBase/spStream.h"

#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateIndexBuffer()
        {
            return std::make_unique<spIndexBuffer>();
        }

        const spRTTIRecord IndexBufferRecord{
            spIndexBuffer::ClassID,
            spBaseObject::ClassID,
            "spIndexBuffer",
            &spBaseObject::StaticRTTI(),
            &CreateIndexBuffer,
            nullptr,
        };

        const bool IndexBufferRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(IndexBufferRecord);
    }

    spIndexBuffer::~spIndexBuffer()
    {
        ReleaseForAnalysis();
    }

    const spRTTIRecord& spIndexBuffer::StaticRTTI() noexcept
    {
        (void)IndexBufferRegistered;
        return IndexBufferRecord;
    }

    std::unique_ptr<spBaseObject> spIndexBuffer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spIndexBuffer>();
        manager.RegisterClone(*this, *clone);

        // PC 0x0045FA50 / PS2 0x00159960 route through the root copy slot;
        // index storage is not copied by the RTTI clone operation.
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spIndexBuffer::vfunc_18() const noexcept
    {
        return IndexBufferRecord;
    }

    std::optional<std::uint32_t> spIndexBuffer::IndexCountFor(
        const eIndexBufferType type,
        const std::uint32_t primitiveCount) noexcept
    {
        switch (type)
        {
        case eIndexBufferType::Type1:
            return primitiveCount;
        case eIndexBufferType::Type2:
            if (primitiveCount > std::numeric_limits<std::uint32_t>::max() / 3)
            {
                return std::nullopt;
            }
            return primitiveCount * 3;
        case eIndexBufferType::Type3:
            if (primitiveCount > std::numeric_limits<std::uint32_t>::max() - 2)
            {
                return std::nullopt;
            }
            return primitiveCount + 2;
        case eIndexBufferType::Type4:
            if (primitiveCount > std::numeric_limits<std::uint32_t>::max() / 2)
            {
                return std::nullopt;
            }
            return primitiveCount * 2;
        default:
            return std::nullopt;
        }
    }

    bool spIndexBuffer::InitializeForAnalysis(
        const std::uint32_t primitiveCount,
        const eIndexBufferType type,
        const std::uint32_t formatFlags)
    {
        const auto indexCount = IndexCountFor(type, primitiveCount);
        if (!indexCount.has_value())
        {
            return false;
        }

        try
        {
            std::vector<std::uint32_t> replacement(*indexCount, 0);
            indices_ = std::move(replacement);
        }
        catch (...)
        {
            return false;
        }

        type_ = type;
        primitiveCount_ = primitiveCount;
        indexCount_ = *indexCount;
        formatFlags_ = formatFlags;
        initialized_ = true;
        return true;
    }

    void spIndexBuffer::ReleaseForAnalysis() noexcept
    {
        // vector::shrink_to_fit may allocate/throw despite the native release
        // path and this facade both being noexcept. Swapping with an empty
        // standard-allocator vector deterministically releases ownership.
        std::vector<std::uint32_t>{}.swap(indices_);
        initialized_ = false;

        // Native release clears only +0x24 and +0x10.  Counts, type and flags
        // remain observable, so the portable state deliberately retains them.
    }

    bool spIndexBuffer::ReadForAnalysis(spStream& stream, const std::uint32_t maximumSerializedBytes)
    {
        std::uint32_t rawType = 0;
        std::uint32_t primitiveCount = 0;
        std::uint32_t formatFlags = 0;
        if (maximumSerializedBytes < 12 || !stream.Read(rawType)
            || !stream.Read(primitiveCount)
            || !stream.Read(formatFlags))
        {
            return false;
        }
        const auto count=IndexCountFor(static_cast<eIndexBufferType>(rawType),primitiveCount);
        const std::uint32_t width=(formatFlags&1u)?4u:2u;
        if(!count || *count>(maximumSerializedBytes-12)/width ||
            !InitializeForAnalysis(primitiveCount,static_cast<eIndexBufferType>(rawType),formatFlags))return false;

        const auto elementSize = GetIndexElementSizeForAnalysis();
        for (std::uint32_t index = 0; index < indexCount_; ++index)
        {
            if (elementSize == sizeof(std::uint16_t))
            {
                std::uint16_t value = 0;
                if (!stream.Read(value))
                {
                    return false;
                }
                indices_[index] = value;
            }
            else if (!stream.Read(indices_[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool spIndexBuffer::WriteForAnalysis(spStream& stream) const
    {
        if (!initialized_)
        {
            return false;
        }
        const auto rawType = static_cast<std::uint32_t>(type_);
        if (!stream.Write(rawType)
            || !stream.Write(primitiveCount_)
            || !stream.Write(formatFlags_))
        {
            return false;
        }

        const auto elementSize = GetIndexElementSizeForAnalysis();
        for (const auto value : indices_)
        {
            if (elementSize == sizeof(std::uint16_t))
            {
                if (value > std::numeric_limits<std::uint16_t>::max())
                {
                    return false;
                }
                const auto narrowed = static_cast<std::uint16_t>(value);
                if (!stream.Write(narrowed))
                {
                    return false;
                }
            }
            else if (!stream.Write(value))
            {
                return false;
            }
        }
        return true;
    }

    std::unique_ptr<spIndexBuffer> spIndexBuffer::CopyBufferForAnalysis() const
    {
        auto copy = std::make_unique<spIndexBuffer>();
        if (!initialized_)
        {
            return copy;
        }
        if (!copy->InitializeForAnalysis(primitiveCount_, type_, formatFlags_))
        {
            return nullptr;
        }
        copy->indices_ = indices_;
        return copy;
    }

    bool spIndexBuffer::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    spIndexBuffer::eIndexBufferType spIndexBuffer::GetTypeForAnalysis() const noexcept
    {
        return type_;
    }

    std::uint32_t spIndexBuffer::GetPrimitiveCountForAnalysis() const noexcept
    {
        return primitiveCount_;
    }

    std::uint32_t spIndexBuffer::GetIndexCountForAnalysis() const noexcept
    {
        return indexCount_;
    }

    std::uint32_t spIndexBuffer::GetFormatFlagsForAnalysis() const noexcept
    {
        return formatFlags_;
    }

    std::size_t spIndexBuffer::GetIndexElementSizeForAnalysis() const noexcept
    {
        return (formatFlags_ & 1U) != 0
            ? sizeof(std::uint32_t)
            : sizeof(std::uint16_t);
    }

    bool spIndexBuffer::SetIndexForAnalysis(
        const std::uint32_t position,
        const std::uint32_t value) noexcept
    {
        if (!initialized_ || position >= indices_.size()
            || (GetIndexElementSizeForAnalysis() == sizeof(std::uint16_t)
                && value > std::numeric_limits<std::uint16_t>::max()))
        {
            return false;
        }
        indices_[position] = value;
        return true;
    }

    std::optional<std::uint32_t> spIndexBuffer::GetIndexForAnalysis(
        const std::uint32_t position) const noexcept
    {
        if (!initialized_ || position >= indices_.size())
        {
            return std::nullopt;
        }
        return indices_[position];
    }
}
