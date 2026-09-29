#include "spEntity.h"

#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        spEntityHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateEntity()
        {
            if (!factoryHost) throw std::logic_error("spEntity requires a factory host");
            return std::make_unique<spEntity>(*factoryHost);
        }
        const spRTTIRecord record{spEntity::ClassID, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), &CreateEntity, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    spEntity::spEntity(spEntityHost& host) : host_(host)
    {
        host_.AddToEngineManagerForAnalysis(*this);
    }

    spEntity::~spEntity()
    {
        host_.DestroyEntityForAnalysis(*this);
    }

    void spEntity::SetFactoryHostForAnalysis(spEntityHost* host) noexcept
    {
        factoryHost = host;
    }

    const spRTTIRecord& spEntity::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& spEntity::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> spEntity::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spEntity>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool spEntity::vfunc_14(spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spNamedObject::vfunc_14(destination, manager);
    }

    void spEntity::SetReferenceForAnalysis(void* next)
    {
        if (reference18_ == next) return;
        if (reference18_)
        {
            const auto count = static_cast<std::uint16_t>(
                host_.GetReferenceCountForAnalysis(reference18_) - 1u);
            host_.SetReferenceCountForAnalysis(reference18_, count);
            if (count == 0) host_.DeleteReferenceForAnalysis(reference18_);
        }
        if (next)
        {
            const auto count = static_cast<std::uint16_t>(
                host_.GetReferenceCountForAnalysis(next) + 1u);
            host_.SetReferenceCountForAnalysis(next, count);
        }
        reference18_ = next;
    }
}
