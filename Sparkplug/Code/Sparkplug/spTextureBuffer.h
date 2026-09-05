#pragma once

// Inferred declaration path. Public getter spellings survive in the native
// texture-data serializer, while this portable reconstruction deliberately
// keeps the unresolved auxiliary-object type out of its writable API.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTextureBuffer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x205B390B;
        static constexpr std::uint32_t DefaultPixelFormat = 6;

        spTextureBuffer() noexcept = default;
        ~spTextureBuffer() override;

        spTextureBuffer(const spTextureBuffer&) = delete;
        spTextureBuffer& operator=(const spTextureBuffer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Hardware-independent equivalent of native Init(width, height,
        // depth, auxiliaryObject, pixelFormat). The exact source spelling of
        // the third dimension and the +0x24 object type remains unresolved.
        // This safe facade always supplies a null auxiliary object.
        [[nodiscard]] bool InitializeForAnalysis(
            std::uint16_t width,
            std::uint16_t height,
            std::uint16_t depth,
            std::uint32_t pixelFormat);
        void ReleaseForAnalysis() noexcept;

        [[nodiscard]] static std::uint32_t PixelSizeForFormatForAnalysis(
            std::uint32_t pixelFormat) noexcept;
        [[nodiscard]] bool SetDataForAnalysis(
            const std::vector<std::byte>& bytes);

        [[nodiscard]] std::uint32_t GetWidthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetHeightForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetDepthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetPixelFormatForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetPixelSizeForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<std::byte>&
            GetBufferForAnalysis() const noexcept;
        [[nodiscard]] bool HasAuxiliaryObjectForAnalysis() const noexcept;
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;

    private:
        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        std::uint32_t depth_ = 0;
        std::vector<std::byte> buffer_;
        std::uint32_t pixelFormat_ = DefaultPixelFormat;
        bool hasAuxiliaryObject_ = false;
        std::uint32_t pixelSize_ = 0;
        bool initialized_ = false;
    };
}
