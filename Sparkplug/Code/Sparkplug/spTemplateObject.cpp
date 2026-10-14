#include "spTemplateObject.h"
#include "spNode.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTemplateObject()
        {
            return std::make_unique<spTemplateObject>();
        }

        const spRTTIRecord TemplateObjectRecord{
            spTemplateObject::ClassID,
            spNamedObject::ClassID,
            "spTemplateObject",
            &spNamedObject::StaticRTTI(),
            &CreateTemplateObject,
            nullptr,
        };

        const bool TemplateObjectRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TemplateObjectRecord);
    }

    spTemplateObject::spTemplateObject() = default;

    spTemplateObject::~spTemplateObject() = default;

    const spRTTIRecord& spTemplateObject::StaticRTTI() noexcept
    {
        (void)TemplateObjectRegistered;
        return TemplateObjectRecord;
    }

    std::unique_ptr<spBaseObject> spTemplateObject::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplateObject>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spTemplateObject::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        (void)manager;
        if (!destination.IsKindOf(ClassID))
        {
            return false;
        }

        auto& clone = static_cast<spTemplateObject&>(destination);

        // Native PC 0x005FD720 / PS2 0x00157070 copy these members but do
        // not invoke spNamedObject's copy routine. Constructor-only members
        // (+0x14, +0x24/+0x28 and the P/R/S tail at +0x174) therefore retain
        // the destination's state. A factory-created clone starts with defaults,
        // whereas copying into an existing descriptor preserves its placement.
        clone.loadedObject_ = loadedObject_;
        clone.state_ = state_;
        clone.resourcePath_ = resourcePath_;
        clone.field12C_ = field12C_;
        clone.field130_ = field130_;
        clone.field134_ = field134_;
        return true;
    }

    const spRTTIRecord& spTemplateObject::vfunc_18() const noexcept
    {
        return TemplateObjectRecord;
    }

    bool spTemplateObject::SetSerializedDescriptorForAnalysis(
        const std::uint32_t state,
        const std::string_view path)
    {
        if (state >= NativeStateCount || path.size() >= NativePathCapacity)
        {
            return false;
        }
        state_ = state;
        resourcePath_ = std::string(path);
        return true;
    }

    bool spTemplateObject::HasSerializedDescriptorForAnalysis() const noexcept
    {
        return state_.has_value() && resourcePath_.has_value();
    }

    std::optional<std::uint32_t>
        spTemplateObject::GetNativeStateForAnalysis() const noexcept
    {
        return state_;
    }

    const char* spTemplateObject::GetResourcePathForAnalysis() const noexcept
    {
        return resourcePath_.has_value() ? resourcePath_->c_str() : nullptr;
    }

    void spTemplateObject::SetLoadedObjectForAnalysis(
        std::shared_ptr<spBaseObject> object) noexcept
    {
        loadedObject_ = std::move(object);
    }

    spBaseObject* spTemplateObject::GetLoadedObjectForAnalysis() const noexcept
    {
        return loadedObject_.get();
    }

    std::uint32_t spTemplateObject::GetField12CForAnalysis() const noexcept
    {
        return field12C_;
    }

    std::int32_t spTemplateObject::GetField130ForAnalysis() const noexcept
    {
        return field130_;
    }

    const spTemplateObject::Vector3& spTemplateObject::GetPositionForAnalysis() const noexcept
    {
        return position_;
    }

    const spTemplateObject::Matrix3& spTemplateObject::GetOrientationForAnalysis() const noexcept
    {
        return orientation_;
    }

    const spTemplateObject::Vector3& spTemplateObject::GetScaleForAnalysis() const noexcept
    {
        return scale_;
    }

    void spTemplateObject::SetPlacementForAnalysis(const Vector3& position,
        const Matrix3& orientation, const Vector3& scale) noexcept
    {
        position_ = position;
        orientation_ = orientation;
        scale_ = scale;
    }

    void spTemplateObject::ApplyNodePlacementForAnalysis(spNode& node) const noexcept
    {
        // PC005FBBE6..005FBC8B (state 0), 005FBABF..005FBB65 (state 3),
        // and 005FBDA5..005FBE55 (actor Node) copy the descriptor's P/R/S
        // separately, with Node's dirty bit 1 set after each group.
        node.SetPositionForAnalysis(position_);
        node.MarkLocalTransformDirtyForAnalysis();
        node.SetOrientationForAnalysis(orientation_);
        node.MarkLocalTransformDirtyForAnalysis();
        node.SetScaleForAnalysis(scale_);
        node.MarkLocalTransformDirtyForAnalysis();
    }
}
