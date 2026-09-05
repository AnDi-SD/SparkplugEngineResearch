#include "spGameLevel.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateGameLevel()
        {
            return std::make_unique<spGameLevel>();
        }

        const spRTTIRecord GameLevelRecord{
            spGameLevel::ClassID,
            spNamedObject::ClassID,
            "spGameLevel",
            &spNamedObject::StaticRTTI(),
            &CreateGameLevel,
            nullptr,
        };

        const bool GameLevelRegistered =
            spRTTIManager::Instance().Register(GameLevelRecord);
    }

    spGameLevel::spGameLevel() noexcept = default;

    spGameLevel::~spGameLevel() = default;

    const spRTTIRecord& spGameLevel::StaticRTTI() noexcept
    {
        (void)GameLevelRegistered;
        return GameLevelRecord;
    }

    std::unique_ptr<spBaseObject> spGameLevel::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spGameLevel>();
        manager.RegisterClone(*this, *clone);

        // Native clone constructs an empty level and dispatches directly to
        // spNamedObject's copy implementation. Runtime instances/resources
        // remain constructor-fresh while the level name is retained.
        return spNamedObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spGameLevel::vfunc_18() const noexcept
    {
        return GameLevelRecord;
    }

    bool spGameLevel::AddInstanceForAnalysis(
        std::unique_ptr<spTemplateInstance> instance)
    {
        if (instance == nullptr)
        {
            return false;
        }
        instances_.push_back(std::move(instance));
        return true;
    }

    std::unique_ptr<spTemplateInstance> spGameLevel::RemoveInstanceForAnalysis(
        spTemplateInstance& instance) noexcept
    {
        const auto found = std::find_if(
            instances_.begin(),
            instances_.end(),
            [&instance](const auto& candidate)
            {
                return candidate.get() == &instance;
            });
        if (found == instances_.end())
        {
            return nullptr;
        }
        auto result = std::move(*found);
        instances_.erase(found);
        return result;
    }

    void spGameLevel::ClearInstancesForAnalysis() noexcept
    {
        instances_.clear();
    }

    std::size_t spGameLevel::GetInstanceCountForAnalysis() const noexcept
    {
        return instances_.size();
    }
}
