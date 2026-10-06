#include "spTemplate.h"
#include "spTemplateInstance.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spTemplate>(); }
        const spRTTIRecord Record{spTemplate::ClassID, spNamedObject::ClassID,
            "spTemplate", &spNamedObject::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spTemplate::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& spTemplate::vfunc_18() const noexcept { return Record; }
    bool spTemplate::AddObjectForAnalysis(std::shared_ptr<spTemplateObject> object)
    {
        // HOST rejects a duplicate descriptor in this list; the native API is not
        // reconstructed by this injection method.
        if (!object || std::find(objects_.begin(), objects_.end(), object) != objects_.end()) return false;
        objects_.push_back(std::move(object)); return true;
    }
    std::optional<bool> spTemplate::DependsOnTemplateForAnalysis(const spTemplate& target) const noexcept
    { return DependsOnTemplateForAnalysis(target, 0); }
    std::optional<bool> spTemplate::DependsOnTemplateForAnalysis(
        const spTemplate& target, std::uint32_t depth) const noexcept
    {
        if (depth >= 64) return std::nullopt; // HOST guard, absent in original.
        for (const auto& object : objects_)
        {
            const auto state = object->GetNativeStateForAnalysis();
            if (!state) return std::nullopt;
            if (*state != 3) continue;
            const auto* instance = dynamic_cast<const spTemplateInstance*>(object->GetLoadedObjectForAnalysis());
            const auto* nested = instance ? instance->GetTemplateOwnerForAnalysis() : nullptr;
            if (!nested) return std::nullopt;
            if (nested == &target || nested->resourcePath_ == target.resourcePath_) return true;
            const auto result = nested->DependsOnTemplateForAnalysis(target, depth + 1);
            if (!result || *result) return result;
        }
        return false;
    }
    std::unique_ptr<spBaseObject> spTemplate::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplate>(); manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spTemplate::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<spTemplate*>(&destination);
        if (!target) return false; // HOST typed boundary around native cast.
        const auto dependency = DependsOnTemplateForAnalysis(*target);
        if (!dependency || *dependency) return false;
        target->ClearObjectsForAnalysis();
        target->field20_ = field20_;
        // PC00412BE0 always clones each descriptor; it is not the mapped
        // CloneReference path. Native null clones are skipped in list order.
        for (const auto& object : objects_)
        {
            auto cloned = manager.Clone(*object);
            if (!cloned) continue;
            auto* typed = dynamic_cast<spTemplateObject*>(cloned.get());
            if (!typed) return false; // HOST requires the real descriptor type.
            cloned.release(); target->objects_.emplace_back(typed);
        }
        // Neither native copy calls Named copy or copies the resource path,
        // loaded state, runtime root node, buffers or constructor-only fields.
        return true;
    }
}
