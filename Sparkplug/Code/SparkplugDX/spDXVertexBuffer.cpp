#include "spDXVertexBuffer.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXVertexBuffer()
        {
            return std::make_unique<spDXVertexBuffer>();
        }

        const spRTTIRecord DXVertexBufferRecord{
            spDXVertexBuffer::ClassID,
            spBaseObject::ClassID,
            "spDXVertexBuffer",
            &spBaseObject::StaticRTTI(),
            &CreateDXVertexBuffer,
            nullptr,
        };

        const bool DXVertexBufferRegistered =
            spRTTIManager::Instance().Register(DXVertexBufferRecord);
    }

    spDXVertexBuffer::~spDXVertexBuffer() = default;

    const spRTTIRecord& spDXVertexBuffer::StaticRTTI() noexcept
    {
        (void)DXVertexBufferRegistered;
        return DXVertexBufferRecord;
    }

    std::unique_ptr<spBaseObject> spDXVertexBuffer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXVertexBuffer>();
        manager.RegisterClone(*this, *clone);

        // Native 0x004B1EA0 invokes the root-copy slot only. The Direct3D
        // resource and its local metadata are deliberately not copied.
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXVertexBuffer::vfunc_18() const noexcept
    {
        return DXVertexBufferRecord;
    }

    bool spDXVertexBuffer::InitializeForAnalysis(
        const std::uint32_t byteSize,
        const std::uint32_t usage,
        const std::uint32_t fvfCode,
        const std::uint32_t pool)
    {
        std::vector<std::byte> replacement;
        try
        {
            replacement.resize(byteSize);
        }
        catch (...)
        {
            return false;
        }

        data_ = std::move(replacement);
        byteSize_ = byteSize;
        usage_ = usage;
        fvfCode_ = fvfCode;
        pool_ = pool;
        initialized_ = true;
        return true;
    }

    void spDXVertexBuffer::ReleaseDeviceBufferForAnalysis() noexcept
    {
        data_.clear();
        initialized_ = false;
    }

    bool spDXVertexBuffer::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    std::uint32_t spDXVertexBuffer::GetByteSizeForAnalysis() const noexcept
    {
        return byteSize_;
    }

    std::uint32_t spDXVertexBuffer::GetUsageForAnalysis() const noexcept
    {
        return usage_;
    }

    std::uint32_t spDXVertexBuffer::GetFVFCodeForAnalysis() const noexcept
    {
        return fvfCode_;
    }

    std::uint32_t spDXVertexBuffer::GetPoolForAnalysis() const noexcept
    {
        return pool_;
    }

    std::vector<std::byte>& spDXVertexBuffer::GetDataForAnalysis() noexcept
    {
        return data_;
    }

    const std::vector<std::byte>&
    spDXVertexBuffer::GetDataForAnalysis() const noexcept
    {
        return data_;
    }
}
