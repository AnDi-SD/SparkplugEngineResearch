#include "spNode.h"

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
            spRTTIManager::Instance().Register(NodeRecord);
    }

    spNode::~spNode()
    {
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
            if (!nodeDestination.AttachChildForAnalysis(std::move(ownedChild)))
            {
                return false;
            }
        }

        // Collision objects are not exposed until spCollisionInfo is
        // reconstructed. Native copy deep-clones that separate vector too.
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

    bool spNode::IsEnabledForAnalysis() const noexcept
    {
        return (flags_ & EnabledMask) != 0;
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

        child->parent_ = this;
        children_.push_back(std::move(child));
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
