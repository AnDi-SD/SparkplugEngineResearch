#include "spTemplateInstance.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTemplateInstance()
        {
            return std::make_unique<spTemplateInstance>();
        }

        const spRTTIRecord TemplateInstanceRecord{
            spTemplateInstance::ClassID,
            spNamedObject::ClassID,
            "spTemplateInstance",
            &spNamedObject::StaticRTTI(),
            &CreateTemplateInstance,
            nullptr,
        };

        const bool TemplateInstanceRegistered =
            spRTTIManager::Instance().Register(TemplateInstanceRecord);
    }

    spTemplateInstance::spTemplateInstance()
        : instanceRoot_(std::make_unique<spNode>())
    {
        instanceRoot_->SetName("Instance Root");
    }

    spTemplateInstance::~spTemplateInstance() = default;

    const spRTTIRecord& spTemplateInstance::StaticRTTI() noexcept
    {
        (void)TemplateInstanceRegistered;
        return TemplateInstanceRecord;
    }

    std::unique_ptr<spBaseObject> spTemplateInstance::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplateInstance>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spTemplateInstance::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native clone reconstructs an empty root/list and then invokes the
        // inherited spNamedObject copy path. Runtime relationships are absent.
        return destination.IsKindOf(ClassID)
            && spNamedObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spTemplateInstance::vfunc_18() const noexcept
    {
        return TemplateInstanceRecord;
    }

    const spNode& spTemplateInstance::GetInstanceRootForAnalysis() const noexcept
    {
        return *instanceRoot_;
    }

    std::size_t spTemplateInstance::GetAttachedObjectCountForAnalysis() const noexcept
    {
        return attachedObjects_.size();
    }
}
