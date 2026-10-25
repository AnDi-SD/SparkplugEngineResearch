#pragma once

// Inferred header path. The PC executable proves the class identity and
// implementation, but preserves no original source/header filename for this
// small Direct3D wrapper.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXIndexBuffer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x23022413;

        spDXIndexBuffer() noexcept = default;
        ~spDXIndexBuffer() override;

        spDXIndexBuffer(const spDXIndexBuffer&) = delete;
        spDXIndexBuffer& operator=(const spDXIndexBuffer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Safe host analogue of native 0x004B2140. Native forwards these four
        // words to IDirect3DDevice9::CreateIndexBuffer in this exact order.
        [[nodiscard]] bool InitializeForAnalysis(
            std::uint32_t byteSize,
            std::uint32_t usage,
            std::uint32_t format,
            std::uint32_t pool);
        void ReleaseDeviceBufferForAnalysis() noexcept; // CPU shadow only.

        // Actual PC4B2140/4B1FF0 protocol, separate from the CPU shadow above.
        // Release context must outlive the object. Create writes the owned slot directly, ignores HRESULT and stores
        // size. Reinitialization does NOT Release the previous handle first.
        using DeviceCreateForAnalysis=std::int32_t (*)(void*,std::uint32_t,std::uint32_t,
            std::uint32_t,std::uint32_t,std::uintptr_t*) noexcept;
        using DeviceReleaseForAnalysis=std::uint32_t (*)(void*,std::uintptr_t) noexcept;
        [[nodiscard]] bool CreateDeviceBufferForAnalysis(std::uint32_t byteSize,std::uint32_t usage,
            std::uint32_t format,std::uint32_t pool,DeviceCreateForAnalysis,DeviceReleaseForAnalysis,void*) noexcept;
        // Native clears field10 only when COM Release returns0. A nonzero
        // remaining referencecount preserves the stored pointer.
        [[nodiscard]] bool ReleaseNativeDeviceBufferForAnalysis() noexcept;
        [[nodiscard]] std::uintptr_t DeviceBufferForAnalysis() const noexcept{return deviceBuffer_;}

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetByteSizeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetUsageForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFormatForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetPoolForAnalysis() const noexcept;
        [[nodiscard]] std::vector<std::byte>& GetDataForAnalysis() noexcept;
        [[nodiscard]] const std::vector<std::byte>& GetDataForAnalysis() const noexcept;

    private:
        std::uint32_t byteSize_ = 0;
        std::uint32_t usage_ = 0;
        std::uint32_t format_ = 0;
        std::uint32_t pool_ = 0;
        std::vector<std::byte> data_;
        bool initialized_ = false;
        std::uintptr_t deviceBuffer_=0;
        DeviceReleaseForAnalysis deviceRelease_=nullptr;
        void* deviceContext_=nullptr;
    };
}
