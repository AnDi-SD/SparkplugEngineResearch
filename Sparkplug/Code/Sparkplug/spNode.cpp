#include "spNode.h"
#include "spCollisionInfo.h"
#include "../../Analysis/PC/spNodeTransformMath.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateNode()
        {
            return std::make_unique<spNode>();
        }

        const spRTTIRecord NodeRecord{
            spNode::ClassID,
            spNamedObject::ClassID,
            "spNode",
            &spNamedObject::StaticRTTI(),
            &CreateNode,
            nullptr,
        };

        const bool NodeRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(NodeRecord);
    }

    spNode::~spNode()
    {
        ClearCollisionsForAnalysis();
        ClearChildrenForAnalysis();
    }

    const spRTTIRecord& spNode::StaticRTTI() noexcept
    {
        (void)NodeRegistered;
        return NodeRecord;
    }

    std::unique_ptr<spBaseObject> spNode::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spNode>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spNode::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID))
        {
            return false;
        }

        auto& nodeDestination = static_cast<spNode&>(destination);
        nodeDestination.ClearCollisionsForAnalysis();
        nodeDestination.ClearChildrenForAnalysis();
        nodeDestination.parent_ = nullptr;

        if (!spNamedObject::vfunc_14(destination, manager))
        {
            return false;
        }

        // Both binaries copy flags, the local 3x3 orientation, position and
        // scale, but deliberately rebuild parent and cached world state.
        nodeDestination.flags_ = flags_;
        nodeDestination.orientation_ = orientation_;
        nodeDestination.position_ = position_;
        nodeDestination.scale_ = scale_;

        for(const auto& collision:collisions_)
        {
            auto raw=collision->vfunc_10(manager);
            auto* typed=dynamic_cast<spCollisionInfo*>(raw.get());
            if(!typed)return false;
            std::shared_ptr<spCollisionInfo> owner(static_cast<spCollisionInfo*>(raw.release()));
            manager.RegisterSharedCloneForAnalysis(*collision,owner);
            if(!nodeDestination.AttachCollisionForAnalysis(std::move(owner)))return false;
        }

        for (const auto& child : children_)
        {
            if (child == nullptr)
            {
                continue;
            }

            auto childCloneBase = child->vfunc_10(manager);
            auto* childClone = dynamic_cast<spNode*>(childCloneBase.get());
            if (childClone == nullptr)
            {
                return false;
            }

            std::shared_ptr<spNode> ownedChild(
                static_cast<spNode*>(childCloneBase.release()));
            // Supply the host owner for a later map-aware Skin bone hit.
            // The original map itself still borrows this Node pointer.
            manager.RegisterSharedCloneForAnalysis(*child,ownedChild);
            if (!nodeDestination.AttachChildForAnalysis(std::move(ownedChild)))
            {
                return false;
            }
        }

        return true;
    }

    const spRTTIRecord& spNode::vfunc_18() const noexcept
    {
        return NodeRecord;
    }

    const spNode::Vector3& spNode::GetPositionForAnalysis() const noexcept
    {
        return position_;
    }

    void spNode::SetPositionForAnalysis(const Vector3& value) noexcept
    {
        position_ = value;
    }

    const spNode::Vector3& spNode::GetScaleForAnalysis() const noexcept
    {
        return scale_;
    }

    void spNode::SetScaleForAnalysis(const Vector3& value) noexcept
    {
        scale_ = value;
    }

    const spNode::Matrix3& spNode::GetOrientationForAnalysis() const noexcept
    {
        return orientation_;
    }

    void spNode::SetOrientationForAnalysis(const Matrix3& value) noexcept
    {
        orientation_ = value;
    }

    std::uint32_t spNode::GetFlagsForAnalysis() const noexcept
    {
        return flags_;
    }

    void spNode::MarkLocalTransformDirtyForAnalysis() noexcept
    {
        flags_ |= 1U;
    }

    void spNode::SetInheritanceForAnalysis(bool position, bool orientation, bool scale) noexcept
    {
        SetMaskedFlag(flags_, InheritPositionMask, position);
        SetMaskedFlag(flags_, InheritOrientationMask, orientation);
        SetMaskedFlag(flags_, InheritScaleMask, scale);
    }

    const spNode::Vector3& spNode::GetWorldPositionForAnalysis() const noexcept { return worldPosition_; }
    const spNode::Vector3& spNode::GetWorldScaleForAnalysis() const noexcept { return worldScale_; }
    const spNode::Matrix3& spNode::GetWorldOrientationForAnalysis() const noexcept { return worldOrientation_; }
    spNode::Matrix4 spNode::GetWorldMatrixForAnalysis() const noexcept
    {
        return evidence::pc::node_math::Affine(worldPosition_, worldOrientation_, worldScale_);
    }

    spNode::Vector3 spNode::TransformPointToWorldForAnalysis(const Vector3& point) const noexcept
    {
        const double x=double(point[0])*worldScale_[0],y=double(point[1])*worldScale_[1],z=double(point[2])*worldScale_[2];
        const auto& m=worldOrientation_;
        // 4206A1/4206C0/4206DC store all three rotated values before adding
        // position. Y/Z accumulate z,x,y, whereas X accumulates z,y,x.
        Vector3 rotated{static_cast<float>((z*m[6]+y*m[3])+x*m[0]),
            static_cast<float>((z*m[7]+x*m[1])+y*m[4]),static_cast<float>((z*m[8]+x*m[2])+y*m[5])};
        for(std::size_t i=0;i<3;++i)rotated[i]=static_cast<float>(double(rotated[i])+worldPosition_[i]);
        return rotated;
    }

    bool spNode::UpdateWorldForAnalysis(
        const std::uint32_t inheritedFlags, const Matrix3* cameraOrientation) noexcept
    {
        namespace math = evidence::pc::node_math;
        const auto billboard = (flags_ & (BillboardAxis1Mask | BillboardAxis2Mask)) >> 20;
        if (billboard && !math::Billboard(cameraOrientation, billboard, worldOrientation_))
            return false;
        if ((flags_ | inheritedFlags) & 1U)
        {
            if (parent_)
            {
                if (flags_ & InheritPositionMask)
                {
                    auto local = position_;
                    if (flags_ & InheritScaleMask)
                        for (std::size_t i=0;i<3;++i) local[i] *= parent_->worldScale_[i];
                    worldPosition_ = math::Transform(local, parent_->worldOrientation_);
                    for (std::size_t i=0;i<3;++i) worldPosition_[i] += parent_->worldPosition_[i];
                }
                // Without 0x10000, the native updater retains old world position.
                worldScale_ = scale_;
                if (flags_ & InheritScaleMask)
                    for (std::size_t i=0;i<3;++i) worldScale_[i] *= parent_->worldScale_[i];
                if (flags_ & InheritOrientationMask)
                {
                    if (!billboard)
                        worldOrientation_ = math::Multiply(orientation_, parent_->worldOrientation_);
                }
                else
                    worldOrientation_ = orientation_; // also overwrites a computed billboard
            }
            else
            {
                worldPosition_ = position_;
                worldScale_ = scale_;
                if (!billboard) worldOrientation_ = orientation_;
            }
            for(const auto& collision:collisions_)
                if(!collision->UpdateWorldForAnalysis())return false;
        }
        for (const auto& child : children_)
            if (child && !child->UpdateWorldForAnalysis((flags_ | inheritedFlags) & ~2U, cameraOrientation))
                return false;
        flags_ &= ~7U;
        return true;
    }

    bool spNode::IsEnabledForAnalysis() const noexcept
    {
        return (flags_ & EnabledMask) != 0;
    }

    spCollisionInfo* spNode::GetCollisionForAnalysis(std::size_t index) const noexcept
    {return index<collisions_.size()?collisions_[index].get():nullptr;}
    bool spNode::AttachCollisionForAnalysis(std::shared_ptr<spCollisionInfo> collision)
    {
        // Host guard: native blindly overwrites the back pointer and appends.
        // Reject a duplicate/reparent to avoid two direct deletion owners.
        if(!collision||collision->node_)return false;
        collisions_.push_back(collision);collision->node_=this;return true;
    }
    std::shared_ptr<spCollisionInfo> spNode::DetachCollisionForAnalysis(spCollisionInfo& collision) noexcept
    {
        const auto at=std::find_if(collisions_.begin(),collisions_.end(),
            [&](const auto& entry){return entry.get()==&collision;});
        if(at==collisions_.end())return {};
        auto owner=*at;
        *at=std::move(collisions_.back());collisions_.pop_back(); // native swap-with-last
        collision.node_=nullptr;return owner;
    }
    void spNode::ClearCollisionsForAnalysis() noexcept
    {
        for(const auto& collision:collisions_)collision->node_=nullptr;
        collisions_.clear();
    }

    bool spNode::IsHierarchyActiveForAnalysis() const noexcept
    {
        return (flags_ & ActiveHierarchyMask) != 0;
    }

    void spNode::SetHierarchyActiveForAnalysis(const bool value) noexcept
    {
        SetMaskedFlag(flags_, ActiveHierarchyMask, value);
        for (const auto& child : children_)
            if (child) child->SetHierarchyActiveForAnalysis(value);
    }

    bool spNode::IsStaticForAnalysis() const noexcept
    {
        return (flags_ & StaticMask) != 0;
    }

    bool spNode::IsAnimatedForAnalysis() const noexcept
    {
        return (flags_ & AnimatedMask) != 0;
    }

    bool spNode::IsBoneForAnalysis() const noexcept
    {
        return (flags_ & BoneMask) != 0;
    }

    std::uint32_t spNode::GetBillboardAxisForAnalysis() const noexcept
    {
        if ((flags_ & BillboardAxis2Mask) != 0)
        {
            return 2;
        }
        return (flags_ & BillboardAxis1Mask) != 0 ? 1U : 0U;
    }

    void spNode::SetStaticForAnalysis(const bool value) noexcept
    {
        SetMaskedFlag(flags_, StaticMask, value);
    }

    void spNode::SetAnimatedForAnalysis(const bool value) noexcept
    {
        SetMaskedFlag(flags_, AnimatedMask, value);
    }

    void spNode::SetBoneForAnalysis(const bool value) noexcept
    {
        SetMaskedFlag(flags_, BoneMask, value);
    }

    void spNode::SetBillboardAxisForAnalysis(const std::uint32_t value) noexcept
    {
        flags_ &= ~(BillboardAxis1Mask | BillboardAxis2Mask);
        if (value == 1)
        {
            flags_ |= BillboardAxis1Mask;
        }
        else if (value == 2)
        {
            flags_ |= BillboardAxis2Mask;
        }
    }

    void spNode::SetEnabledForAnalysis(
        const bool value,
        const bool recursive) noexcept
    {
        SetMaskedFlag(flags_, EnabledMask, value);
        if (!recursive)
        {
            return;
        }

        for (const auto& child : children_)
        {
            if (child != nullptr)
            {
                child->SetEnabledForAnalysis(value, true);
            }
        }
    }

    spNode* spNode::GetParentForAnalysis() noexcept
    {
        return parent_;
    }

    const spNode* spNode::GetParentForAnalysis() const noexcept
    {
        return parent_;
    }

    spNode* spNode::GetRootForAnalysis() noexcept
    {
        auto* root = this;
        while (root->parent_ != nullptr)
        {
            root = root->parent_;
        }
        return root;
    }

    const spNode* spNode::GetRootForAnalysis() const noexcept
    {
        auto* root = this;
        while (root->parent_ != nullptr)
        {
            root = root->parent_;
        }
        return root;
    }

    std::size_t spNode::GetChildCountForAnalysis() const noexcept
    {
        return children_.size();
    }

    spNode* spNode::GetChildForAnalysis(const std::size_t index) noexcept
    {
        return index < children_.size() ? children_[index].get() : nullptr;
    }

    const spNode* spNode::GetChildForAnalysis(const std::size_t index) const noexcept
    {
        return index < children_.size() ? children_[index].get() : nullptr;
    }

    bool spNode::AttachChildForAnalysis(std::shared_ptr<spNode> child)
    {
        // Native421A7B returns immediately for the same parent: success/no-op,
        // not a duplicate list entry. Different-parent reparenting and scene
        // registrations still need their own reconstructed host contracts.
        if (child && child->parent_ == this) return true;
        if (child == nullptr || child.get() == this || child->parent_ != nullptr)
        {
            return false;
        }

        // Attaching an ancestor below its descendant would create a cycle.
        for (auto* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent_)
        {
            if (ancestor == child.get())
            {
                return false;
            }
        }

        children_.push_back(child); // allocate before mutating reciprocal state
        child->parent_ = this;
        child->flags_ |= 5U; // native421AF7: local/structural world invalidation
        if (IsHierarchyActiveForAnalysis()) child->SetHierarchyActiveForAnalysis(true);
        return true;
    }

    std::shared_ptr<spNode> spNode::DetachChildForAnalysis(
        spNode& child) noexcept
    {
        const auto iterator = std::find_if(
            children_.begin(),
            children_.end(),
            [&child](const auto& candidate)
            {
                return candidate.get() == &child;
            });
        if (iterator == children_.end())
        {
            return nullptr;
        }

        auto detached = std::move(*iterator);
        children_.erase(iterator);
        detached->parent_ = nullptr;
        return detached;
    }

    void spNode::SetMaskedFlag(
        std::uint32_t& flags,
        const std::uint32_t mask,
        const bool value) noexcept
    {
        if (value)
        {
            flags |= mask;
        }
        else
        {
            flags &= ~mask;
        }
    }

    void spNode::ClearChildrenForAnalysis() noexcept
    {
        for (const auto& child : children_)
        {
            if (child != nullptr && child->parent_ == this)
            {
                child->parent_ = nullptr;
            }
        }
        children_.clear();
    }
}
