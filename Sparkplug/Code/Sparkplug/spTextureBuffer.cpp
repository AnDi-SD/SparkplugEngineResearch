#include "spTextureBuffer.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextureBuffer()
        {
            return std::make_unique<spTextureBuffer>();
        }

        const spRTTIRecord TextureBufferRecord{
            spTextureBuffer::ClassID,
            spBaseObject::ClassID,
            "spTextureBuffer",
            &spBaseObject::StaticRTTI(),
            &CreateTextureBuffer,
            nullptr,
        };

        const bool TextureBufferRegistered =
            spRTTIManager::Instance().Register(TextureBufferRecord);
    }

    spTextureBuffer::~spTextureBuffer()
    {
        ReleaseForAnalysis();
    }

    const spRTTIRecord& spTextureBuffer::StaticRTTI() noexcept
    {
        (void)TextureBufferRegistered;
        return TextureBufferRecord;
    }

    std::unique_ptr<spBaseObject> spTextureBuffer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTextureBuffer>();
        manager.RegisterClone(*this, *clone);

        // Both native RTTI clone paths create a constructor-blank object and
        // invoke only the spBaseObject copy slot. Pixel storage is not copied.
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTextureBuffer::vfunc_18() const noexcept
    {
        return TextureBufferRecord;
    }

    std::uint32_t spTextureBuffer::PixelSizeForFormatForAnalysis(
        const std::uint32_t pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case 0:
        case 1:
            return 4;
        case 2:
            return 1;
        case 3:
        case 4:
            return 2;
        default:
            return 0;
        }
    }

    bool spTextureBuffer::InitializeForAnalysis(
        const std::uint16_t width,
        const std::uint16_t height,
        const std::uint16_t depth,
        const std::uint32_t pixelFormat)
    {
        // Native Init releases both owned pointers before validating the new
        // format, then writes these scalar arguments. Preserve that order and
        // the native sticky +0x28 value on an invalid format.
        ReleaseForAnalysis();
        width_ = width;
        height_ = height;
        depth_ = depth;
        pixelFormat_ = pixelFormat;
        hasAuxiliaryObject_ = false;

        const auto pixelSize = PixelSizeForFormatForAnalysis(pixelFormat);
        if (pixelSize == 0)
        {
            return false;
        }
        pixelSize_ = pixelSize;

        const auto byteCount = static_cast<std::uint64_t>(width_)
            * height_ * depth_ * pixelSize_;
        if (byteCount > std::numeric_limits<std::uint32_t>::max()
            || byteCount > std::numeric_limits<std::size_t>::max())
        {
            return false;
        }

        try
        {
            std::vector<std::byte> replacement(
                static_cast<std::size_t>(byteCount), std::byte{0});
            buffer_ = std::move(replacement);
        }
        catch (...)
        {
            return false;
        }
        initialized_ = true;
        return true;
    }

    void spTextureBuffer::ReleaseForAnalysis() noexcept
    {
        std::vector<std::byte>{}.swap(buffer_);
        hasAuxiliaryObject_ = false;
        initialized_ = false;

        // Native release/destruction leaves the dimension, format and pixel-
        // size scalar words untouched while clearing both ownership slots.
    }

    bool spTextureBuffer::SetDataForAnalysis(
        const std::vector<std::byte>& bytes)
    {
        if (!initialized_ || bytes.size() != buffer_.size())
        {
            return false;
        }
        buffer_ = bytes;
        return true;
    }

    std::uint32_t spTextureBuffer::GetWidthForAnalysis() const noexcept
    {
        return width_;
    }

    std::uint32_t spTextureBuffer::GetHeightForAnalysis() const noexcept
    {
        return height_;
    }

    std::uint32_t spTextureBuffer::GetDepthForAnalysis() const noexcept
    {
        return depth_;
    }

    std::uint32_t spTextureBuffer::GetPixelFormatForAnalysis() const noexcept
    {
        return pixelFormat_;
    }

    std::uint32_t spTextureBuffer::GetPixelSizeForAnalysis() const noexcept
    {
        return pixelSize_;
    }

    const std::vector<std::byte>&
        spTextureBuffer::GetBufferForAnalysis() const noexcept
    {
        return buffer_;
    }

    bool spTextureBuffer::HasAuxiliaryObjectForAnalysis() const noexcept
    {
        return hasAuxiliaryObject_;
    }

    bool spTextureBuffer::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }
}
