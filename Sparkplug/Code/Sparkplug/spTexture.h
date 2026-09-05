#pragma once

// Inferred declaration path.  The exact serializer translation unit
// Code/Sparkplug/spTextureDataSerializer.cpp survives in both executables,
// but no original spTexture header/source path has been recovered.

#include "spResource.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spTexture : public spResource
    {
    public:
        static constexpr spClassID ClassID = 0x2F281E13;

        struct InitializationPlan final
        {
            std::uint32_t sourceWidth = 0;
            std::uint32_t sourceHeight = 0;
            std::uint32_t effectiveWidth = 0;
            std::uint32_t effectiveHeight = 0;
            std::uint8_t field1C = 0;
            std::uint32_t textureFlags = 0;
            bool normalizeDimensions = false;
            bool dimensionsUnchanged = false;
        };

        ~spTexture() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native spTexture has no RTTI factory and its clone slot returns
        // null.  The four-method spITexture backend remains abstract in the
        // binaries; its unresolved source signatures are not guessed here.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Hardware-independent part of the two native Init overloads.  The
        // returned plan deliberately stops before the spITexture backend call.
        [[nodiscard]] static std::uint32_t NormalizeDimensionForAnalysis(
            std::uint32_t dimension) noexcept;
        [[nodiscard]] static InitializationPlan PlanInitializationForAnalysis(
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t field1C,
            std::uint32_t textureFlags,
            bool normalizeDimensions) noexcept;
        [[nodiscard]] InitializationPlan ApplyBufferStateForAnalysis(
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t field1C,
            std::uint32_t textureFlags,
            bool normalizeDimensions) noexcept;

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetField18ForAnalysis() const noexcept;
        [[nodiscard]] std::uint8_t GetField1CForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetTextureFlagsForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetWidthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetHeightForAnalysis() const noexcept;
        [[nodiscard]] bool WereDimensionsUnchangedForAnalysis() const noexcept;
        [[nodiscard]] bool GetField31ForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetField34ForAnalysis() const noexcept;

    protected:
        spTexture() noexcept;

    private:
        // Portable state mirrors proven native offsets semantically, not ABI.
        // Native constructors leave +0x18/+0x28/+0x2C unset until Init; this
        // safe host facade initializes them to zero.
        std::uint32_t field18_ = 0;
        std::uint8_t field1C_ = 0;
        std::uint32_t textureFlags_ = 0;
        bool initialized_ = false;
        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        bool dimensionsUnchanged_ = false;
        bool field31_ = false;
        std::uint32_t field34_ = 0;
    };
}
