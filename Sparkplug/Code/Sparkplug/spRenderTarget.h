#pragma once

// Inferred common declaration path. The shipped binaries prove the class,
// hierarchy and reset-facing operation names, but no original header path.

#include "spResource.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spTexture;

    // The original enum token survives in PS2 diagnostics. Enumerator names
    // do not, so the numerical labels deliberately remain neutral.
    enum class eTBPixelFormat : std::uint32_t
    {
        Format0 = 0,
        Format1 = 1,
        Format2 = 2,
        Format3 = 3,
        Format4 = 4,
        Format5 = 5,
        Unknown6 = 6,
    };

    class spRenderTarget : public spResource
    {
    public:
        static constexpr spClassID ClassID = 0x00D1229C;
        static constexpr std::uint32_t DefaultDimension = 256;

        ~spRenderTarget() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // These spellings are present in native reset diagnostics. The
        // portable implementation models lifecycle/state only; backend API
        // objects remain the responsibility of platform leaves.
        [[nodiscard]] virtual bool Init(
            std::uint32_t width,
            std::uint32_t height,
            eTBPixelFormat pixelFormat) = 0;
        [[nodiscard]] virtual bool ReinitTargetsForDeviceReset();
        virtual void ReleaseTargetsForDeviceReset() noexcept;

        [[nodiscard]] std::uint32_t GetWidthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetHeightForAnalysis() const noexcept;
        [[nodiscard]] eTBPixelFormat GetPixelFormatForAnalysis() const noexcept;
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] virtual bool HasBackendTargetForAnalysis() const noexcept;
        [[nodiscard]] spTexture* GetBackingTextureForAnalysis() const noexcept;
        void SetBackingTextureForAnalysis(spTexture* texture) noexcept;

    protected:
        explicit spRenderTarget(
            std::uint32_t width = DefaultDimension,
            std::uint32_t height = DefaultDimension,
            eTBPixelFormat pixelFormat = eTBPixelFormat::Format0) noexcept;

        void SetCommonStateForAnalysis(
            std::uint32_t width,
            std::uint32_t height,
            eTBPixelFormat pixelFormat,
            bool initialized) noexcept;

    private:
        std::uint32_t width_;
        std::uint32_t height_;
        eTBPixelFormat pixelFormat_;
        bool initialized_ = false;
        // Native +0x18 is an intrusive relationship. The portable analysis
        // facade keeps a non-owning pointer until spTexture ownership itself
        // is reconstructed across every platform backend.
        spTexture* backingTexture_ = nullptr;
    };
}
