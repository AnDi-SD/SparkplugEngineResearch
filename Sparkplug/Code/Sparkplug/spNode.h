#pragma once

// Inferred declaration path. The original serializer translation unit is
// proven as Code/Sparkplug/spNodeSerializer.cpp, while no original spNode
// header or implementation path survives in either shipped executable.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNode : public spNamedObject
    {
    public:
        using Vector3 = std::array<float, 3>;
        using Matrix3 = std::array<float, 9>;

        static constexpr spClassID ClassID = 0x695C0F65;

        // Exact native masks used by spNodeSerializer on both platforms.
        static constexpr std::uint32_t EnabledMask = 0x00000200;
        static constexpr std::uint32_t StaticMask = 0x00000400;
        static constexpr std::uint32_t AnimatedMask = 0x00000800;
        static constexpr std::uint32_t BoneMask = 0x00001000;
        static constexpr std::uint32_t BillboardAxis1Mask = 0x00100000;
        static constexpr std::uint32_t BillboardAxis2Mask = 0x00200000;
        static constexpr std::uint32_t NativeDefaultFlags = 0x00070A00;

        spNode() noexcept = default;
        ~spNode() override;

        spNode(const spNode&) = delete;
        spNode& operator=(const spNode&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable names for the executable-backed local transform state.
        // Native setters also maintain cached world state and scene-manager
        // registrations; those side effects remain outside this safe slice.
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept;
        void SetPositionForAnalysis(const Vector3& value) noexcept;
        [[nodiscard]] const Vector3& GetScaleForAnalysis() const noexcept;
        void SetScaleForAnalysis(const Vector3& value) noexcept;
        [[nodiscard]] const Matrix3& GetOrientationForAnalysis() const noexcept;
        void SetOrientationForAnalysis(const Matrix3& value) noexcept;

        [[nodiscard]] std::uint32_t GetFlagsForAnalysis() const noexcept;
        [[nodiscard]] bool IsEnabledForAnalysis() const noexcept;
        [[nodiscard]] bool IsStaticForAnalysis() const noexcept;
        [[nodiscard]] bool IsAnimatedForAnalysis() const noexcept;
        [[nodiscard]] bool IsBoneForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetBillboardAxisForAnalysis() const noexcept;
        void SetStaticForAnalysis(bool value) noexcept;
        void SetAnimatedForAnalysis(bool value) noexcept;
        void SetBoneForAnalysis(bool value) noexcept;
        void SetBillboardAxisForAnalysis(std::uint32_t value) noexcept;

        // PS2 0x001A5B00 and its PC counterpart set bit 0x200 and optionally
        // recurse over the complete child list. The analytical wrapper keeps
        // that exact behavior while avoiding any renderer interaction.
        void SetEnabledForAnalysis(bool value, bool recursive = true) noexcept;

        [[nodiscard]] spNode* GetParentForAnalysis() noexcept;
        [[nodiscard]] const spNode* GetParentForAnalysis() const noexcept;
        [[nodiscard]] spNode* GetRootForAnalysis() noexcept;
        [[nodiscard]] const spNode* GetRootForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetChildCountForAnalysis() const noexcept;
        [[nodiscard]] spNode* GetChildForAnalysis(std::size_t index) noexcept;
        [[nodiscard]] const spNode* GetChildForAnalysis(std::size_t index) const noexcept;

        // Native child pointers are intrusive references. shared_ptr is a
        // host-only ownership substitute and does not claim ABI equivalence.
        [[nodiscard]] bool AttachChildForAnalysis(std::shared_ptr<spNode> child);
        [[nodiscard]] std::shared_ptr<spNode> DetachChildForAnalysis(
            spNode& child) noexcept;

    private:
        static void SetMaskedFlag(
            std::uint32_t& flags,
            std::uint32_t mask,
            bool value) noexcept;
        void ClearChildrenForAnalysis() noexcept;

        Vector3 position_{0.0F, 0.0F, 0.0F};
        Matrix3 orientation_{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
        Vector3 scale_{1.0F, 1.0F, 1.0F};
        std::uint32_t flags_ = NativeDefaultFlags;
        spNode* parent_ = nullptr;
        std::vector<std::shared_ptr<spNode>> children_;
    };
}
