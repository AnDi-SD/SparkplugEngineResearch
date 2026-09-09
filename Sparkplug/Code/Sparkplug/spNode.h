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
    class spCollisionInfo;
    class spScene;
    class spNode : public spNamedObject
    {
        friend class spLight; // PC Light serializers share the native Node dirty word.
    public:
        using Vector3 = std::array<float, 3>;
        using Matrix3 = std::array<float, 9>;
        using Matrix4 = std::array<float, 16>;

        static constexpr spClassID ClassID = 0x695C0F65;

        // Exact native masks used by spNodeSerializer on both platforms.
        static constexpr std::uint32_t EnabledMask = 0x00000200;
        static constexpr std::uint32_t ActiveHierarchyMask = 0x00000100;
        static constexpr std::uint32_t StaticMask = 0x00000400;
        static constexpr std::uint32_t AnimatedMask = 0x00000800;
        static constexpr std::uint32_t BoneMask = 0x00001000;
        static constexpr std::uint32_t BillboardAxis1Mask = 0x00100000;
        static constexpr std::uint32_t BillboardAxis2Mask = 0x00200000;
        static constexpr std::uint32_t NativeDefaultFlags = 0x00070A00;
        static constexpr std::uint32_t InheritPositionMask = 0x00010000;
        static constexpr std::uint32_t InheritOrientationMask = 0x00020000;
        static constexpr std::uint32_t InheritScaleMask = 0x00040000;

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
        // These raw analytical setters do not imply native invalidation or
        // scene registrations. Mark dirty explicitly before updating caches.
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept;
        void SetPositionForAnalysis(const Vector3& value) noexcept;
        [[nodiscard]] const Vector3& GetScaleForAnalysis() const noexcept;
        void SetScaleForAnalysis(const Vector3& value) noexcept;
        [[nodiscard]] const Matrix3& GetOrientationForAnalysis() const noexcept;
        void SetOrientationForAnalysis(const Matrix3& value) noexcept;

        [[nodiscard]] std::uint32_t GetFlagsForAnalysis() const noexcept;
        // Borrowed native3C field. Explicit pointer seam only; Scene registry
        // attachment/notification is outside this class's reconstructed slice.
        [[nodiscard]] spScene* GetSceneForAnalysis() const noexcept{return scene_;}
        // Proven by PC position/scale consumers and the resolved quaternion
        // setter. Cached state changes only when UpdateWorld is called.
        void MarkLocalTransformDirtyForAnalysis() noexcept;
        void SetInheritanceForAnalysis(bool position, bool orientation, bool scale) noexcept;
        [[nodiscard]] const Vector3& GetWorldPositionForAnalysis() const noexcept;
        [[nodiscard]] const Vector3& GetWorldScaleForAnalysis() const noexcept;
        [[nodiscard]] const Matrix3& GetWorldOrientationForAnalysis() const noexcept;
        [[nodiscard]] Matrix4 GetWorldMatrixForAnalysis() const noexcept;

        // PC vslot +0x30. Reconstructs represented transform/cache/child state.
        // Represented collision transforms update before descendants. Scene
        // registrations/queries are outside the tools slice. Camera is explicit.
        // Returns false for a degenerate camera (host safety). A failure in a
        // descendant can occur after earlier nodes were updated.
        [[nodiscard]] virtual bool UpdateWorldForAnalysis(
            std::uint32_t inheritedFlags = 0, const Matrix3* cameraOrientation = nullptr) noexcept;
        [[nodiscard]] bool IsEnabledForAnalysis() const noexcept;
        // PC420DE0 changes bit100 recursively, independently of Enabled200.
        // This does NOT attach/detach scene registries or rebuild light caches.
        [[nodiscard]] bool IsHierarchyActiveForAnalysis() const noexcept;
        void SetHierarchyActiveForAnalysis(bool value) noexcept;
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

        // PC421ED0/421690: Node owns each collision directly; the collision
        // retains its primitive separately. Shared ownership bridges the host
        // file cache, with one Node back pointer and no implicit reparenting.
        [[nodiscard]] std::size_t GetCollisionCountForAnalysis() const noexcept{return collisions_.size();}
        [[nodiscard]] spCollisionInfo* GetCollisionForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] bool AttachCollisionForAnalysis(std::shared_ptr<spCollisionInfo> collision);
        [[nodiscard]] std::shared_ptr<spCollisionInfo> DetachCollisionForAnalysis(spCollisionInfo& collision) noexcept;

    private:
        friend class spActor; // original discovery sets ownership bit2000
        friend class spRenderNode; // PC world/bounds dirty bits in the common B0 word
        static void SetMaskedFlag(
            std::uint32_t& flags,
            std::uint32_t mask,
            bool value) noexcept;
        void ClearChildrenForAnalysis() noexcept;
        void ClearCollisionsForAnalysis() noexcept;

        Vector3 position_{0.0F, 0.0F, 0.0F};
        Matrix3 orientation_{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
        Vector3 scale_{1.0F, 1.0F, 1.0F};
        Vector3 worldPosition_{0.0F, 0.0F, 0.0F};
        Vector3 worldScale_{1.0F, 1.0F, 1.0F};
        Matrix3 worldOrientation_{1,0,0,0,1,0,0,0,1};
        std::uint32_t flags_ = NativeDefaultFlags;
        spNode* parent_ = nullptr;
        spScene* scene_ = nullptr;
        std::vector<std::shared_ptr<spNode>> children_;
        std::vector<std::shared_ptr<spCollisionInfo>> collisions_;
    };
}
