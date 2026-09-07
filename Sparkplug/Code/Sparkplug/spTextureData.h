#pragma once

// Inferred declaration path. The exact serializer source path is known, but
// no original spTextureData header/source path survives in the executables.

#include "spTexture.h"
#include "spTextureBuffer.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spTextureData final : public spTexture
    {
    public:
        static constexpr spClassID ClassID = 0x78EA082B;
        // Analytical CPU representation of native vector+6C records; native
        // memory order is width, rows, rowStride, pointer (16 bytes on PC).
        struct NativeMipForAnalysis final
        {
            std::uint32_t width=0,rows=0,rowStride=0;
            std::vector<std::byte> bytes;
        };

        spTextureData() noexcept = default;
        ~spTextureData() override = default;

        spTextureData(const spTextureData&) = delete;
        spTextureData& operator=(const spTextureData&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Safe host equivalent of the confirmed spTexture Init -> virtual
        // copy-to-embedded-buffer path. Platform upload/container operations
        // are intentionally not guessed.
        [[nodiscard]] bool InitializeFromTextureBufferForAnalysis(
            const spTextureBuffer& source,
            std::uint8_t field1C,
            std::uint32_t textureFlags,
            bool normalizeDimensions);

        [[nodiscard]] spTextureBuffer& GetTextureBufferForAnalysis() noexcept;
        [[nodiscard]] const spTextureBuffer&
            GetTextureBufferForAnalysis() const noexcept;

        [[nodiscard]] bool GetField68ForAnalysis() const noexcept;
        void SetField68ForAnalysis(bool value) noexcept;
        [[nodiscard]] bool GetFieldAfterFirstContainerForAnalysis() const noexcept;
        void SetFieldAfterFirstContainerForAnalysis(bool value) noexcept;
        // Bounded caller input preparation, NOT a recovered original Init name.
        // Does not generate/convert any mip; preserves separately supplied CPU buffer.
        [[nodiscard]] bool SetNativeMipDataForAnalysis(std::uint32_t width,std::uint32_t height,
            std::uint32_t flags,std::uint8_t field1C,std::vector<NativeMipForAnalysis> mips);
        [[nodiscard]] const std::vector<NativeMipForAnalysis>& GetNativeMipsForAnalysis() const noexcept{return nativeMips_;}

    private:
        spTextureBuffer textureBuffer_;
        bool field68_ = false;
        bool fieldAfterFirstContainer_ = false;
        std::vector<NativeMipForAnalysis> nativeMips_;
    };
}
