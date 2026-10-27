#pragma once
#include "Code/Sparkplug/spProjectionFX.h"
#include <optional>

namespace sparkplug::reconstruction
{
    class spMaterial;
    // Captured stable foreign RenderNode fields for the4C58D0 gate, not a
    // Scene/Node producer or an atomic snapshot. nullopt means node.scene3C
    // is null; otherwise the value is that Scene's word40.
    struct spProjectionDrawGateInputForAnalysis final
    {
        std::optional<std::uint32_t> sceneStamp40;
        std::uint32_t supportMark118=0;
        std::uint32_t nodeFlagsB0=0;
    };
    enum class spProjectionDrawGateResultForAnalysis
    {
        Skipped,
        UnqualifiedActiveDraw,
    };
    enum class spProjectionMaterialPreparationPrefixResultForAnalysis
    {
        AlreadyInitialized,
        UnqualifiedMissingOwner,
        UnqualifiedMaterialGraph,
        UnqualifiedMaterialLayerCreation,
    };
    class spPCProjectionFX final : public spProjectionFX
    {
    public:
        static constexpr spClassID ClassID = 0xF37E1DC5;
        spPCProjectionFX() noexcept = default;
        ~spPCProjectionFX() override = default;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // PC4C4FF0: successful existing-byte or no-material paths only. These
        // domains never create native cache20; other initialization is refused.
        spProjectionFxInitializationResultForAnalysis InitializeForAnalysis(const spProjectionFxHost&) override;
        // Separate PC4C4FF0 material prefix. For zero1C, caller supplies the
        // same live material bound at owner20, with stable pass/layer/texture
        // lifetimes. Stops before4C5060/StdLayer creation, without setting1C.
        // These analytical statuses are not native false/success returns.
        [[nodiscard]] spProjectionMaterialPreparationPrefixResultForAnalysis
            PrepareMaterialLayersPrefixForAnalysis(spMaterial&) noexcept;
        // Actual primary Draw4C58D0 -> support424790/Enabled200 early gates.
        // Skipped corresponds to native literal true with no draw; active
        // input is refused before camera/matrix/model/renderer work. Neither
        // initialized1C nor owner18 participates in these gates.
        [[nodiscard]] spProjectionDrawGateResultForAnalysis EvaluateDrawGateForAnalysis(
            const spProjectionDrawGateInputForAnalysis&) const noexcept;
        const void* Cache20ForAnalysis() const noexcept { return cache20_; }
    private:
        const void* cache20_ = nullptr; // constructor-proven; no admitted producer
    };
}
