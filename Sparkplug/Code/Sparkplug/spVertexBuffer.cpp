#include "spVertexBuffer.h"

#include "../SparkBase/spStream.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateVertexBuffer()
        {
            return std::make_unique<spVertexBuffer>();
        }

        const spRTTIRecord VertexBufferRecord{
            spVertexBuffer::ClassID,
            spBaseObject::ClassID,
            "spVertexBuffer",
            &spBaseObject::StaticRTTI(),
            &CreateVertexBuffer,
            nullptr,
        };

        const bool VertexBufferRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(VertexBufferRecord);
    }

    spVertexBuffer::~spVertexBuffer()
    {
        ReleaseForAnalysis();
    }

    const spRTTIRecord& spVertexBuffer::StaticRTTI() noexcept
    {
        (void)VertexBufferRegistered;
        return VertexBufferRecord;
    }

    std::unique_ptr<spBaseObject> spVertexBuffer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spVertexBuffer>();
        manager.RegisterClone(*this, *clone);

        // Both native RTTI clone paths leave buffer state constructor-blank.
        // Full payload duplication is the separate CopyBuffer operation.
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spVertexBuffer::vfunc_18() const noexcept
    {
        return VertexBufferRecord;
    }

    void spVertexBuffer::RebuildComponentLayoutForAnalysis() noexcept
    {
        // PC 0x45FEA0 / PS2 0x15C8E0 overwrite only present components.
        // Absent offsets retain the constructor/previous layout values.
        vertexStride_ = 12;
        std::uint16_t nextFloat = 3;

        const auto add = [this, &nextFloat](
            const std::uint32_t bit,
            const std::size_t slot,
            const std::uint16_t width) {
            if ((componentFlags_ & bit) == 0)
            {
                return;
            }
            componentOffsets_[slot] = nextFloat;
            nextFloat = static_cast<std::uint16_t>(nextFloat + width);
            vertexStride_ = static_cast<std::uint16_t>(
                vertexStride_ + width * sizeof(float));
        };

        add(0x000001, 1, 1);
        for (std::size_t index = 0; index < 4; ++index)
        {
            if ((componentFlags_ & (0x000002U << index)) == 0)
            {
                break;
            }
            add(0x000002U << index, 2 + index, 1);
        }
        add(0x000020, 6, 1);
        add(0x000040, 7, 3);
        add(0x000080, 8, 1);
        add(0x000100, 9, 1);
        add(0x000200, 10, 1);
        add(0x000400, 11, 3);
        for (std::size_t index = 0; index < 8; ++index)
        {
            if ((componentFlags_ & (0x000800U << index)) == 0)
            {
                break;
            }
            add(0x000800U << index, 12 + index, 2);
        }
        add(0x080000, 20, 3);
        add(0x100000, 21, 3);
        componentCount_ = nextFloat;
    }

    bool spVertexBuffer::AllocateForCurrentLayoutForAnalysis()
    {
        const auto byteCount = static_cast<std::uint64_t>(vertexStride_)
            * vertexCount_;
        if (byteCount > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        try
        {
            std::vector<std::byte> replacement(
                static_cast<std::size_t>(byteCount), std::byte{0});
            data_ = std::move(replacement);
        }
        catch (...)
        {
            return false;
        }
        vertexSize_ = static_cast<std::uint32_t>(byteCount);
        initialized_ = true;
        return true;
    }

    bool spVertexBuffer::InitializeForAnalysis(
        const std::uint32_t componentFlags,
        const std::uint32_t vertexCount,
        const std::uint32_t flags)
    {
        componentFlags_ = componentFlags;
        vertexCount_ = vertexCount;
        flags_ = flags;
        RebuildComponentLayoutForAnalysis();
        return AllocateForCurrentLayoutForAnalysis();
    }

    bool spVertexBuffer::InitializeFromDataForAnalysis(
        const std::uint32_t componentFlags,
        const std::uint32_t vertexCount,
        const std::uint32_t flags,
        const std::vector<std::byte>& bytes)
    {
        if (!InitializeForAnalysis(componentFlags, vertexCount, flags)
            || bytes.size() != data_.size())
        {
            return false;
        }
        data_ = bytes;
        return true;
    }

    bool spVertexBuffer::InitializeRawForAnalysis(const std::uint32_t byteCount)
    {
        try
        {
            std::vector<std::byte> replacement(byteCount, std::byte{0});
            data_ = std::move(replacement);
        }
        catch (...)
        {
            return false;
        }
        componentFlags_ = 0;
        vertexCount_ = 0;
        flags_ = 0;
        vertexSize_ = byteCount;
        initialized_ = true;

        // Native raw initialization deliberately retains +0x10/+0x18 and the
        // component-offset table when reusing an existing object.
        return true;
    }

    void spVertexBuffer::ReleaseForAnalysis() noexcept
    {
        std::vector<std::byte>{}.swap(data_);
        initialized_ = false;

        // Native release clears only data ownership and the initialized byte.
        // Layout, counts and flags remain in the object.
    }

    bool spVertexBuffer::ReadForAnalysis(spStream& stream, const std::uint32_t maximumSerializedBytes)
    {
        std::uint32_t componentFlags = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t flags = 0;
        if (maximumSerializedBytes < 12 || !stream.Read(componentFlags)
            || !stream.Read(vertexCount)
            || !stream.Read(flags))
        {
            return false;
        }
        spVertexBuffer layout;
        layout.componentFlags_=componentFlags;layout.RebuildComponentLayoutForAnalysis();
        if(vertexCount>(maximumSerializedBytes-12)/layout.vertexStride_ ||
            !InitializeForAnalysis(componentFlags,vertexCount,flags))return false;
        return vertexSize_ == 0
            || stream.ReadData(data_.data(), vertexSize_);
    }

    bool spVertexBuffer::WriteForAnalysis(spStream& stream) const
    {
        if (!initialized_
            || !stream.Write(componentFlags_)
            || !stream.Write(vertexCount_)
            || !stream.Write(flags_))
        {
            return false;
        }
        const auto serializedSize = static_cast<std::uint64_t>(vertexStride_)
            * vertexCount_;
        if (serializedSize > data_.size()
            || serializedSize > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }
        return serializedSize == 0
            || stream.WriteData(
                data_.data(), static_cast<std::uint32_t>(serializedSize));
    }

    std::unique_ptr<spVertexBuffer> spVertexBuffer::CopyBufferForAnalysis() const
    {
        auto copy = std::make_unique<spVertexBuffer>();
        if (!initialized_)
        {
            return copy;
        }
        if (!copy->InitializeFromDataForAnalysis(
                componentFlags_, vertexCount_, flags_, data_))
        {
            return nullptr;
        }
        return copy;
    }

    bool spVertexBuffer::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    std::uint32_t spVertexBuffer::GetComponentFlagsForAnalysis() const noexcept
    {
        return componentFlags_;
    }

    std::uint32_t spVertexBuffer::GetVertexCountForAnalysis() const noexcept
    {
        return vertexCount_;
    }

    std::uint32_t spVertexBuffer::GetFlagsForAnalysis() const noexcept
    {
        return flags_;
    }

    std::uint16_t spVertexBuffer::GetVertexStrideForAnalysis() const noexcept
    {
        return vertexStride_;
    }

    std::uint16_t spVertexBuffer::GetComponentCountForAnalysis() const noexcept
    {
        return componentCount_;
    }

    std::uint32_t spVertexBuffer::GetVertexSizeForAnalysis() const noexcept
    {
        return vertexSize_;
    }

    const std::array<std::uint16_t, spVertexBuffer::ComponentOffsetCount>&
        spVertexBuffer::GetComponentOffsetsForAnalysis() const noexcept
    {
        return componentOffsets_;
    }

    const std::vector<std::byte>& spVertexBuffer::GetDataForAnalysis() const noexcept
    {
        return data_;
    }

    bool spVertexBuffer::SetDataForAnalysis(
        const std::vector<std::byte>& bytes)
    {
        if (!initialized_ || bytes.size() != data_.size())
        {
            return false;
        }
        data_ = bytes;
        return true;
    }
}
