#include "spCamera.h"

#include <cmath>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord CameraRecord{
            spCamera::ClassID,
            spNode::ClassID,
            "spCamera",
            &spNode::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool CameraRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(CameraRecord);

        spCamera::Matrix4 IdentityMatrix() noexcept
        {
            return {
                1.0F, 0.0F, 0.0F, 0.0F,
                0.0F, 1.0F, 0.0F, 0.0F,
                0.0F, 0.0F, 1.0F, 0.0F,
                0.0F, 0.0F, 0.0F, 1.0F,
            };
        }
    }

    const spRTTIRecord& spCamera::StaticRTTI() noexcept
    {
        (void)CameraRegistered;
        return CameraRecord;
    }

    std::unique_ptr<spBaseObject> spCamera::vfunc_10(spCloneManager&) const
    {
        // Both native base vtables return null; only concrete camera leaves
        // allocate a clone.
        return nullptr;
    }

    const spRTTIRecord& spCamera::vfunc_18() const noexcept
    {
        return CameraRecord;
    }

    float spCamera::GetNearClipPlane() const noexcept
    {
        return nearClipPlane_;
    }

    float spCamera::GetFarClipPlane() const noexcept
    {
        return farClipPlane_;
    }

    float spCamera::GetViewAngle() const noexcept
    {
        return viewAngle_;
    }

    float spCamera::GetPixelAspectRatio() const noexcept
    {
        return pixelAspectRatio_;
    }

    bool spCamera::Is2DMode() const noexcept
    {
        return twoDimensional_;
    }

    void spCamera::SetNearClipPlane(const float value) noexcept
    {
        nearClipPlane_ = value;
        cameraDirtyFlags_ |= ProjectionDirtyMask;
    }

    void spCamera::SetFarClipPlane(const float value) noexcept
    {
        farClipPlane_ = value;
        cameraDirtyFlags_ |= ProjectionDirtyMask;
    }

    void spCamera::SetViewAngle(const float value) noexcept
    {
        viewAngle_ = value;
        // Original PC427DBF unconditionally leaves serialized2D mode.
        // The independent projection-branch byte231 is not changed.
        twoDimensional_ = false;
        cameraDirtyFlags_ |= ProjectionDirtyMask;
    }

    void spCamera::SetPixelAspectRatio(const float value) noexcept
    {
        pixelAspectRatio_ = value;
        cameraDirtyFlags_ |= ProjectionDirtyMask;
    }

    void spCamera::Set2DModeForAnalysis(const bool value) noexcept
    {
        // The native setter changes the serialized flag and immediately
        // notifies the renderer. No camera dirty bit is changed.
        twoDimensional_ = value;
    }

    bool spCamera::ConfigureViewportForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const float pixelAspectRatio,
        const std::uint32_t backendMode) noexcept
    {
        // Native callers provide valid non-zero dimensions. The portable
        // facade rejects invalid input to avoid a host divide-by-zero.
        if (width == 0 || height == 0
            || !std::isfinite(pixelAspectRatio)
            || pixelAspectRatio <= 0.0F)
        {
            return false;
        }

        viewportActive_ = true;
        viewportWidth_ = width;
        viewportHeight_ = height;
        viewportRatio_ = static_cast<float>(height)
            / static_cast<float>(width);
        pixelAspectRatio_ = pixelAspectRatio;
        backendMode_ = backendMode;
        cameraDirtyFlags_ |= NativeConfigureDirtyMask;
        return true;
    }

    void spCamera::DeactivateViewportForAnalysis() noexcept
    {
        cameraDirtyFlags_ = 0;
        viewportActive_ = false;
    }

    bool spCamera::IsViewportActiveForAnalysis() const noexcept
    {
        return viewportActive_;
    }

    std::uint32_t spCamera::GetViewportWidthForAnalysis() const noexcept
    {
        return viewportWidth_;
    }

    std::uint32_t spCamera::GetViewportHeightForAnalysis() const noexcept
    {
        return viewportHeight_;
    }

    float spCamera::GetViewportRatioForAnalysis() const noexcept
    {
        return viewportRatio_;
    }

    std::uint32_t spCamera::GetBackendModeForAnalysis() const noexcept
    {
        return backendMode_;
    }

    std::uint32_t spCamera::GetCameraDirtyFlagsForAnalysis() const noexcept
    {
        return cameraDirtyFlags_;
    }

    bool spCamera::BuildProjectionMatrixForAnalysis(
        Matrix4& destination,
        const bool orthographicBranch) const noexcept
    {
        const float halfExtent = nearClipPlane_
            * std::tan(viewAngle_ * 0.5F);
        const float aspectExtent = pixelAspectRatio_
            * viewportRatio_ * halfExtent;
        const float depth = farClipPlane_ - nearClipPlane_;
        if (!std::isfinite(halfExtent) || !std::isfinite(aspectExtent)
            || !std::isfinite(depth) || halfExtent == 0.0F
            || aspectExtent == 0.0F || depth == 0.0F)
        {
            return false;
        }

        destination = IdentityMatrix();
        if (orthographicBranch)
        {
            destination[0] = 2.0F / halfExtent;
            destination[5] = 2.0F / aspectExtent;
            destination[10] = 1.0F / depth;
            destination[14] = nearClipPlane_ / -depth;
            return true;
        }

        destination[0] = nearClipPlane_ / halfExtent;
        destination[5] = nearClipPlane_ / aspectExtent;
        destination[10] = farClipPlane_ / depth;
        destination[11] = 1.0F;
        destination[14] = -(nearClipPlane_ * farClipPlane_) / depth;
        destination[15] = 0.0F;
        return true;
    }
}
