#include "spDXIndexBuffer.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXIndexBuffer()
        {
            return std::make_unique<spDXIndexBuffer>();
        }

        const spRTTIRecord DXIndexBufferRecord{
            spDXIndexBuffer::ClassID,
            spBaseObject::ClassID,
            "spDXIndexBuffer",
            &spBaseObject::StaticRTTI(),
            &CreateDXIndexBuffer,
            nullptr,
        };

        const bool DXIndexBufferRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXIndexBufferRecord);
    }

    spDXIndexBuffer::~spDXIndexBuffer()
    {
        // Actual4B20D0 releases a nonnull handle once on destruction, even if
        // an earlier4B1FF0 kept it after a nonzero Release referencecount.
        if(deviceBuffer_&&deviceRelease_)(void)deviceRelease_(deviceContext_,deviceBuffer_);
    }

    const spRTTIRecord& spDXIndexBuffer::StaticRTTI() noexcept
    {
        (void)DXIndexBufferRegistered;
        return DXIndexBufferRecord;
    }

    std::unique_ptr<spBaseObject> spDXIndexBuffer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXIndexBuffer>();
        manager.RegisterClone(*this, *clone);

        // Native 0x004B2080 invokes the root-copy slot only. The Direct3D
        // resource and its local metadata are deliberately not copied.
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXIndexBuffer::vfunc_18() const noexcept
    {
        return DXIndexBufferRecord;
    }

    bool spDXIndexBuffer::InitializeForAnalysis(
        const std::uint32_t byteSize,
        const std::uint32_t usage,
        const std::uint32_t format,
        const std::uint32_t pool)
    {
        if(deviceRelease_)return false; // Explicit host/COM modes do not mix.
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
        format_ = format;
        pool_ = pool;
        initialized_ = true;
        return true;
    }

    bool spDXIndexBuffer::CreateDeviceBufferForAnalysis(std::uint32_t byteSize,std::uint32_t usage,
        std::uint32_t format,std::uint32_t pool,DeviceCreateForAnalysis create,DeviceReleaseForAnalysis release,void* context) noexcept
    {
        if(initialized_||!create||!release||(deviceRelease_&&(deviceRelease_!=release||deviceContext_!=context)))return false;
        deviceRelease_=release;deviceContext_=context;
        (void)create(context,byteSize,usage,format,pool,&deviceBuffer_);
        byteSize_=byteSize;usage_=usage;format_=format;pool_=pool;
        return true; // Original AL=1, not HRESULT success.
    }
    bool spDXIndexBuffer::ReleaseNativeDeviceBufferForAnalysis() noexcept
    {
        if(!deviceBuffer_)return true;
        if(!deviceRelease_)return false;
        if(deviceRelease_(deviceContext_,deviceBuffer_)==0)deviceBuffer_=0;
        return true; // Analytical completion only; native has no bool contract.
    }

    void spDXIndexBuffer::ReleaseDeviceBufferForAnalysis() noexcept
    {
        data_.clear();
        initialized_ = false;
    }

    bool spDXIndexBuffer::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    std::uint32_t spDXIndexBuffer::GetByteSizeForAnalysis() const noexcept
    {
        return byteSize_;
    }

    std::uint32_t spDXIndexBuffer::GetUsageForAnalysis() const noexcept
    {
        return usage_;
    }

    std::uint32_t spDXIndexBuffer::GetFormatForAnalysis() const noexcept
    {
        return format_;
    }

    std::uint32_t spDXIndexBuffer::GetPoolForAnalysis() const noexcept
    {
        return pool_;
    }

    std::vector<std::byte>& spDXIndexBuffer::GetDataForAnalysis() noexcept
    {
        return data_;
    }

    const std::vector<std::byte>&
    spDXIndexBuffer::GetDataForAnalysis() const noexcept
    {
        return data_;
    }
}
