#pragma once

// The class identity and PC member layout are executable-backed. The portable
// containers and ForAnalysis names do not claim the lost original header ABI.

#include "spModel.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNode;

    class spSkin final : public spModel
    {
    public:
        static constexpr spClassID ClassID = 0x681F2043;
        using Matrix4 = std::array<float, 16>;

        struct BoneBinding final
        {
            std::shared_ptr<spNode> bone;
            Matrix4 inverseBindMatrix{};

            [[nodiscard]] bool operator==(const BoneBinding& other) const noexcept;
        };

        spSkin() noexcept = default;
        ~spSkin() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(
            spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x0046A100 assigns boneCount plus two parallel arrays. This safe
        // facade rejects shapes the native reader itself cannot construct.
        [[nodiscard]] bool SetPaletteForAnalysis(
            std::uint32_t weightCount,
            std::vector<BoneBinding> bindings);
        [[nodiscard]] std::uint32_t GetWeightCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetBoneCountForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<BoneBinding>&
            GetBoneBindingsForAnalysis() const noexcept;

    private:
        std::uint32_t weightCount_ = 0;
        std::vector<BoneBinding> boneBindings_;
    };
}
