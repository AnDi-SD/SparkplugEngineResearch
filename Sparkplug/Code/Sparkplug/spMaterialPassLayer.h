#pragma once

// Inferred declaration path. Both shipped executables preserve the native
// class name, identity and complete fixed-capacity layout, but no source path.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spMaterialTextureLayer;

    class spMaterialPassLayer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x3A8905A5;
        static constexpr std::size_t MaximumLayerCount = 8;

        spMaterialPassLayer() noexcept = default;
        ~spMaterialPassLayer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] std::uint32_t GetFinalBlendOperationForAnalysis()
            const noexcept;
        void SetFinalBlendOperationForAnalysis(std::uint32_t operation) noexcept;
        [[nodiscard]] std::size_t GetLayerCountForAnalysis() const noexcept;
        [[nodiscard]] const std::shared_ptr<spMaterialTextureLayer>&
            GetLayerForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] bool SetLayerForAnalysis(std::size_t index,
            std::shared_ptr<spMaterialTextureLayer> layer) noexcept;

    private:
        std::uint32_t finalBlendOperation_ = 0;
        std::size_t layerCount_ = 0;
        std::array<std::shared_ptr<spMaterialTextureLayer>, MaximumLayerCount>
            layers_{};
    };
}
