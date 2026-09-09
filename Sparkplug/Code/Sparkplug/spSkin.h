#pragma once

// The class identity and PC member layout are executable-backed. The portable
// containers and ForAnalysis names do not claim the lost original header ABI.

#include "spModel.h"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
namespace sparkplug::evidence::pc {struct SkinRenderContextForAnalysis;}

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
            BoneBinding(std::shared_ptr<spNode> bone,Matrix4 matrix)
                : inverseBindMatrix(std::move(matrix)),ownedBone_(std::move(bone)) {}
            Matrix4 inverseBindMatrix{};

            // Native Skin borrows Node pointers. Loaded palettes use weak
            // references to their context/graph owners, avoiding ancestor cycles.
            // Explicitly constructed host palettes and cloned orphan bones may
            // retain owners; neither is claimed as native intrusive ownership.
            [[nodiscard]] static BoneBinding BorrowedForAnalysis(
                const std::shared_ptr<spNode>& bone,Matrix4 matrix)
            {BoneBinding result(nullptr,std::move(matrix));result.borrowedBone_=bone;return result;}
            [[nodiscard]] std::shared_ptr<spNode> GetBoneForAnalysis() const noexcept
            {return ownedBone_?ownedBone_:borrowedBone_.lock();}
            void SetOwnedBoneForAnalysis(std::shared_ptr<spNode> bone) noexcept
            {ownedBone_=std::move(bone);borrowedBone_.reset();}

            [[nodiscard]] bool operator==(const BoneBinding& other) const noexcept;
        private:
            std::shared_ptr<spNode> ownedBone_;
            std::weak_ptr<spNode> borrowedBone_;
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

        // PC 0x0046A2AA calls 0x00426B00 with inverseBind in ECX and the
        // already-assembled world matrix as the second stack argument.
        // This does not compute world transforms from local spNode state.
        [[nodiscard]] static Matrix4 ComposePaletteMatrixForAnalysis(
            const Matrix4& inverseBind, const Matrix4& boneWorld) noexcept;
        // PC46A240 publishes identity after building world-space bone palettes.
        // Shared with the reconstructed render body and host inspection bridge.
        [[nodiscard]] static constexpr Matrix4 GetRenderWorldMatrixForAnalysis() noexcept
        { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
        // PC46A240 complete palette -> world setter -> mesh/pass/draw chain,
        // with declared alpha/fog/light/material and shader inputs.
        // Context carries explicit renderer/SDK inputs, no live GPU.
        [[nodiscard]] bool RenderForAnalysis(
            sparkplug::evidence::pc::SkinRenderContextForAnalysis&,spCamera*,void* support);
        // Compatibility name from the initial unlit-only reconstruction.
        [[nodiscard]] bool RenderUnlitForAnalysis(
            sparkplug::evidence::pc::SkinRenderContextForAnalysis& state,spCamera* camera,void* support)
        { return RenderForAnalysis(state,camera,support); }

    private:
        // Original PC factory 46A120/constructor initializes +60 to four.
        std::uint32_t weightCount_ = 4;
        std::vector<BoneBinding> boneBindings_;
    };
}
