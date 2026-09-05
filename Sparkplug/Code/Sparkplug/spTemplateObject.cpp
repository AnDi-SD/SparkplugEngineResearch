#include "spTemplateObject.h"

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
            spRTTIManager::Instance().Register(TemplateObjectRecord);
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
        // (+0x14, +0x24/+0x28 and +0x174) therefore stay fresh.
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
}
