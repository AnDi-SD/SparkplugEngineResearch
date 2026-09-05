#pragma once

// Inferred common declaration path. Native code proves three separate lists:
// ordinary targets, cube targets, and layer-render-target textures. The last
// dependency is intentionally deferred until its owning texture class is
// reconstructed.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spRenderTarget;
    class spMaterialRenderTargetTexture;

    class spRenderTargetManager : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x546C50F2;

        ~spRenderTargetManager() override;

        spRenderTargetManager(const spRenderTargetManager&) = delete;
        spRenderTargetManager& operator=(const spRenderTargetManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spRenderTargetManager* GetInstance() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool RegisterTargetForAnalysis(spRenderTarget& target);
        [[nodiscard]] bool DeactivateTargetForAnalysis(
            spRenderTarget& target) noexcept;
        [[nodiscard]] bool RegisterLayerTargetForAnalysis(
            spMaterialRenderTargetTexture& target);
        [[nodiscard]] bool DeactivateLayerTargetForAnalysis(
            spMaterialRenderTargetTexture& target) noexcept;
        void ReleaseTargetsForDeviceReset() noexcept;
        [[nodiscard]] bool ReinitTargetsForDeviceReset();

        [[nodiscard]] std::size_t GetOrdinaryTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetCubeTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetActiveOrdinaryTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetActiveCubeTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetLayerTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetActiveLayerTargetCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetCurrentTargetIndexForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetTargetSlotCountForAnalysis() const noexcept;
        [[nodiscard]] bool SetCurrentTargetIndexForAnalysis(
            std::size_t index) noexcept;

    protected:
        spRenderTargetManager() noexcept;

    private:
        struct Entry final
        {
            spRenderTarget* target = nullptr;
            bool active = false;
        };

        struct LayerEntry final
        {
            spMaterialRenderTargetTexture* target = nullptr;
            bool active = false;
        };

        static spRenderTargetManager* instance_;
        std::vector<Entry> ordinaryTargets_;
        std::vector<Entry> cubeTargets_;
        std::vector<LayerEntry> layerTargets_;
        std::size_t currentTargetIndex_ = 0;
        std::size_t targetSlotCount_ = 1;
    };
}
