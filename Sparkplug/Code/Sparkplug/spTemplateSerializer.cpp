#include "spTemplateSerializer.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTemplateSerializer()
        {
            return std::make_unique<spTemplateSerializer>();
        }

        const spRTTIRecord TemplateSerializerRecord{
            spTemplateSerializer::ClassID,
            spBaseObject::ClassID,
            "spTemplateSerializer",
            &spBaseObject::StaticRTTI(),
            &CreateTemplateSerializer,
            nullptr,
        };

        const bool TemplateSerializerRegistered =
            spRTTIManager::Instance().Register(TemplateSerializerRecord);
    }

    spTemplateSerializer::spTemplateSerializer() noexcept = default;

    spTemplateSerializer::~spTemplateSerializer() = default;

    const spRTTIRecord& spTemplateSerializer::StaticRTTI() noexcept
    {
        (void)TemplateSerializerRegistered;
        return TemplateSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTemplateSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplateSerializer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spTemplateSerializer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Both native vtables reuse the root no-payload copy implementation.
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spTemplateSerializer::vfunc_18() const noexcept
    {
        return TemplateSerializerRecord;
    }

    void spTemplateSerializer::BindForAnalysis(
        spTemplateObject* const target,
        spStream* const input) noexcept
    {
        target_ = target;
        input_ = input;
    }

    spTemplateObject* spTemplateSerializer::GetTargetForAnalysis() const noexcept
    {
        return target_;
    }

    spStream* spTemplateSerializer::GetInputForAnalysis() const noexcept
    {
        return input_;
    }

    std::int32_t spTemplateSerializer::GetParentIDForAnalysis() const noexcept
    {
        return parentID_;
    }

    bool spTemplateSerializer::HasOutputForAnalysis() const noexcept
    {
        return output_ != nullptr;
    }
}
