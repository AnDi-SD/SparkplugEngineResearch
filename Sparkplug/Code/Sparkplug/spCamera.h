#pragma once

// Inferred common declaration path. The shipped binaries prove the class,
// hierarchy and behavior below, but preserve no original header/source path.

#include "spNode.h"

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spCamera : public spNode
    {
    public:
        using Matrix4 = std::array<float, 16>;

        static constexpr spClassID ClassID = 0x18DF3845;
        static constexpr std::uint32_t ViewTransformDirtyMask = 0x01;
        static constexpr std::uint32_t ProjectionDirtyMask = 0x02;
        static constexpr std::uint32_t NativeConfigureDirtyMask = 0x7F;
        static constexpr float DefaultNearClipPlane = 1.0F;
        static constexpr float DefaultFarClipPlane = 10000.0F;
        static constexpr float DefaultViewAngle = 1.0471975803375244F;
        static constexpr float DefaultPixelAspectRatio = 1.0F;

        ~spCamera() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] float GetNearClipPlane() const noexcept;
        [[nodiscard]] float GetFarClipPlane() const noexcept;
        [[nodiscard]] float GetViewAngle() const noexcept;
        [[nodiscard]] float GetPixelAspectRatio() const noexcept;
        [[nodiscard]] bool Is2DMode() const noexcept;

        void SetNearClipPlane(float value) noexcept;
        void SetFarClipPlane(float value) noexcept;
        void SetViewAngle(float value) noexcept;
        void SetPixelAspectRatio(float value) noexcept;
        void Set2DModeForAnalysis(bool value) noexcept;

        // This is the confirmed common camera-vtable operation at PC +0x38
        // / PS2 +0x40 (the PS2 table includes two ABI header words). Its
        // original name and final integer parameter name remain unknown.
        // The portable spelling therefore does not claim a recovered symbol.
        [[nodiscard]] bool ConfigureViewportForAnalysis(
            std::uint32_t width,
            std::uint32_t height,
            float pixelAspectRatio,
            std::uint32_t backendMode) noexcept;
        void DeactivateViewportForAnalysis() noexcept;

        [[nodiscard]] bool IsViewportActiveForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetViewportWidthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetViewportHeightForAnalysis() const noexcept;
        [[nodiscard]] float GetViewportRatioForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetBackendModeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetCameraDirtyFlagsForAnalysis() const noexcept;

        // Reproduces the two matrix branches in PC 0x00426F10 and PS2
        // 0x001B17E0/0x001B22F0 without touching either native renderer.
        // `orthographicBranch` names the observed branch only; its original
        // field/API name is still unknown and is not the serialized 2D flag.
        [[nodiscard]] bool BuildProjectionMatrixForAnalysis(
            Matrix4& destination,
            bool orthographicBranch = false) const noexcept;

    protected:
        spCamera() noexcept = default;

    private:
        float nearClipPlane_ = DefaultNearClipPlane;
        float farClipPlane_ = DefaultFarClipPlane;
        float viewAngle_ = DefaultViewAngle;
        float pixelAspectRatio_ = DefaultPixelAspectRatio;
        bool twoDimensional_ = false;
        bool viewportActive_ = false;
        std::uint32_t viewportWidth_ = 0;
        std::uint32_t viewportHeight_ = 0;
        float viewportRatio_ = 1.0F;
        std::uint32_t backendMode_ = 0;
        std::uint32_t cameraDirtyFlags_ = ProjectionDirtyMask;
    };
}
