#include "spEntityManager.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateEntityManager()
        {
            return std::make_unique<spEntityManager>();
        }

        const spRTTIRecord EntityManagerRecord{
            spEntityManager::ClassID,
            spBaseObject::ClassID,
            "spEntityManager",
            &spBaseObject::StaticRTTI(),
            &CreateEntityManager,
            nullptr,
        };

        const bool EntityManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(EntityManagerRecord);
    }

    spEntityManager* spEntityManager::instance_ = nullptr;

    spEntityManager::spEntityManager() noexcept
    {
        instance_ = this;
    }

    spEntityManager::~spEntityManager()
    {
        ClearForAnalysis();
        instance_ = nullptr;
    }

    const spRTTIRecord& spEntityManager::StaticRTTI() noexcept
    {
        (void)EntityManagerRegistered;
        return EntityManagerRecord;
    }

    spEntityManager* spEntityManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spEntityManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spEntityManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spEntityManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Both native clone paths construct an empty runtime manager and call
        // the root no-payload copy slot. Managed entities are not duplicated.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spEntityManager::vfunc_18() const noexcept
    {
        return EntityManagerRecord;
    }

    bool spEntityManager::AddForAnalysis(std::unique_ptr<spBaseObject> entity)
    {
        if (!entity || ContainsForAnalysis(*entity))
        {
            return false;
        }
        entities_.push_back(std::move(entity));
        return true;
    }

    std::unique_ptr<spBaseObject> spEntityManager::RemoveForAnalysis(
        const spBaseObject& entity)
    {
        const auto iterator = std::find_if(
            entities_.begin(), entities_.end(),
            [&entity](const auto& candidate) { return candidate.get() == &entity; });
        if (iterator == entities_.end())
        {
            return nullptr;
        }
        auto result = std::move(*iterator);
        entities_.erase(iterator);
        return result;
    }

    void spEntityManager::ClearForAnalysis() noexcept
    {
        entities_.clear();
    }

    void spEntityManager::DispatchForAnalysis(const void* const notification) noexcept
    {
        // Native code advances before dispatch, which permits the current
        // entity to remove itself without invalidating the traversal cursor.
        for (auto iterator = entities_.begin(); iterator != entities_.end();)
        {
            auto* const entity = iterator++->get();
            if (entity != nullptr)
            {
                entity->vfunc_0C(notification);
            }
        }
    }

    std::size_t spEntityManager::GetEntityCountForAnalysis() const noexcept
    {
        return entities_.size();
    }

    bool spEntityManager::ContainsForAnalysis(const spBaseObject& entity) const noexcept
    {
        return std::any_of(
            entities_.begin(), entities_.end(),
            [&entity](const auto& candidate) { return candidate.get() == &entity; });
    }
}
