#include "spTexture.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        // Registration declares spNamedObject as the base even though the
        // native C++ constructor/destructor chain passes through spResource.
        const spRTTIRecord TextureRecord{
            spTexture::ClassID,
            spNamedObject::ClassID,
            "spTexture",
            &spNamedObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool TextureRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TextureRecord);
    }

    spTexture::spTexture() noexcept = default;

    spTexture::~spTexture() = default;

    const spRTTIRecord& spTexture::StaticRTTI() noexcept
    {
        (void)TextureRegistered;
        return TextureRecord;
    }

    std::unique_ptr<spBaseObject> spTexture::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spTexture::vfunc_18() const noexcept
    {
        return TextureRecord;
    }

    std::uint32_t spTexture::NormalizeDimensionForAnalysis(
        const std::uint32_t dimension) noexcept
    {
        if (dimension == 0)
        {
            return 1;
        }

        std::uint32_t exponent = 1;
        while (exponent < 32
            && (std::uint32_t{1} << exponent) < dimension)
        {
            ++exponent;
        }

        // Both x86 SHL and MIPS SLLV use only the low five bits of a 32-bit
        // shift count.  This preserves the native out-of-domain result too.
        return std::uint32_t{1} << (exponent & 31U);
    }

    spTexture::InitializationPlan spTexture::PlanInitializationForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint8_t field1C,
        const std::uint32_t textureFlags,
        const bool normalizeDimensions) noexcept
    {
        InitializationPlan plan;
        plan.sourceWidth = width;
        plan.sourceHeight = height;
        plan.effectiveWidth = normalizeDimensions
            ? NormalizeDimensionForAnalysis(width)
            : width;
        plan.effectiveHeight = normalizeDimensions
            ? NormalizeDimensionForAnalysis(height)
            : height;
        plan.field1C = field1C;
        plan.textureFlags = textureFlags;
        plan.normalizeDimensions = normalizeDimensions;
        plan.dimensionsUnchanged = plan.effectiveWidth == width
            && plan.effectiveHeight == height;
        return plan;
    }

    spTexture::InitializationPlan spTexture::ApplyBufferStateForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const std::uint8_t field1C,
        const std::uint32_t textureFlags,
        const bool normalizeDimensions) noexcept
    {
        const auto plan = PlanInitializationForAnalysis(
            width, height, field1C, textureFlags, normalizeDimensions);

        // This is the pTextureBuffer overload: native code explicitly writes
        // zero at +0x18 before forwarding to the hardware interface.
        field18_ = 0;
        field1C_ = field1C;
        textureFlags_ = textureFlags;
        initialized_ = true;
        width_ = plan.effectiveWidth;
        height_ = plan.effectiveHeight;
        if (normalizeDimensions)
        {
            dimensionsUnchanged_ = plan.dimensionsUnchanged;
        }
        return plan;
    }

    bool spTexture::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    void spTexture::ApplyRuntimeAttachmentStateForAnalysis(std::uint32_t width,std::uint32_t height) noexcept
    {width_=width;height_=height;initialized_=true;field31_=false;}

    void spTexture::ApplyNativeMipStateForAnalysis(std::uint32_t width,std::uint32_t height,
        std::uint32_t levelCount,std::uint32_t flags,std::uint8_t field1C) noexcept
    {width_=width;height_=height;field18_=levelCount;textureFlags_=flags;field1C_=field1C;initialized_=true;}

    std::uint32_t spTexture::GetField18ForAnalysis() const noexcept
    {
        return field18_;
    }

    std::uint8_t spTexture::GetField1CForAnalysis() const noexcept
    {
        return field1C_;
    }

    std::uint32_t spTexture::GetTextureFlagsForAnalysis() const noexcept
    {
        return textureFlags_;
    }

    std::uint32_t spTexture::GetWidthForAnalysis() const noexcept
    {
        return width_;
    }

    std::uint32_t spTexture::GetHeightForAnalysis() const noexcept
    {
        return height_;
    }

    bool spTexture::WereDimensionsUnchangedForAnalysis() const noexcept
    {
        return dimensionsUnchanged_;
    }

    bool spTexture::GetField31ForAnalysis() const noexcept
    {
        return field31_;
    }

    std::uint32_t spTexture::GetField34ForAnalysis() const noexcept
    {
        return field34_;
    }
}
