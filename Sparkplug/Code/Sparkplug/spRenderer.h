#pragma once

// Inferred common header path. Both shipped executables register spRenderer
// directly below spCrossPlatform, but no original header path survives.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    // Analytical names for the platform-interface operations whose behavior
    // is now proven from their native callers and backend endpoints. These
    // are not claimed to be the original C++ method names.
    enum class spRendererPlatformForAnalysis : std::uint8_t
    {
        PC,
        PS2,
    };

    enum class spRendererPlatformOperationForAnalysis : std::uint8_t
    {
        BindRenderTarget,
        BindCubeRenderTarget,
        BeginScene,
        EndScene,
        Clear,
        SubmitMesh,
        Configure2D,
        SetProjectionMatrix,
        SetViewMatrix,
        SetWorldMatrix,
        SetViewport,
        SetTextureTransform,
        SetFog,
    };

    // Common renderer owner. The native class is abstract (null RTTI factory)
    // and exposes a separate 29-slot platform-render interface at +0x18.
    // That interface is intentionally not named here until its original type
    // or method names are recovered.
    class spRenderer : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x2D9C0296;
        static constexpr std::size_t PlatformInterfaceSlotCount = 29;
        static constexpr std::size_t RenderStateCacheCount = 12;

        ~spRenderer() override;

        spRenderer(const spRenderer&) = delete;
        spRenderer& operator=(const spRenderer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spRenderer* GetInstance() noexcept;

        // PC and PS2 swap the first two render-target operations. The other
        // confirmed camera/frame operations retain their logical ordinals.
        [[nodiscard]] static std::size_t
            GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis platform,
                spRendererPlatformOperationForAnalysis operation) noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x00454940 and PS2 0x00179E60 both invalidate twelve render
        // states. Texture-state capacity is selected by the platform leaf.
        [[nodiscard]] bool InvalidateStateCachesForAnalysis() noexcept;
        [[nodiscard]] std::size_t GetRenderStateCacheCountForAnalysis()
            const noexcept;
        [[nodiscard]] std::size_t GetTextureStateCacheCountForAnalysis()
            const noexcept;
        [[nodiscard]] std::uint32_t GetRenderStateCacheForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] std::uint32_t GetTextureStateCacheForAnalysis(
            std::size_t index) const noexcept;

    protected:
        explicit spRenderer(std::size_t textureStateCacheCount);

    private:
        static spRenderer* instance_;
        std::vector<std::uint32_t> renderStateCache_;
        std::vector<std::uint32_t> textureStateCache_;
    };
}
