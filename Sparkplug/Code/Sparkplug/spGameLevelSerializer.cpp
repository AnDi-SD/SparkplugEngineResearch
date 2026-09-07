#include "spGameLevelSerializer.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateGameLevelSerializer()
        {
            return std::make_unique<spGameLevelSerializer>();
        }

        const spRTTIRecord GameLevelSerializerRecord{
            spGameLevelSerializer::ClassID,
            spBaseObject::ClassID,
            "spGameLevelSerializer",
            &spBaseObject::StaticRTTI(),
            &CreateGameLevelSerializer,
            nullptr,
        };

        const bool GameLevelSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(GameLevelSerializerRecord);
    }

    spGameLevelSerializer::spGameLevelSerializer() noexcept = default;

    spGameLevelSerializer::~spGameLevelSerializer() = default;

    const spRTTIRecord& spGameLevelSerializer::StaticRTTI() noexcept
    {
        (void)GameLevelSerializerRegistered;
        return GameLevelSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spGameLevelSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spGameLevelSerializer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spGameLevelSerializer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native PC/PS2 vtables both reuse root no-payload copy.
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spGameLevelSerializer::vfunc_18() const noexcept
    {
        return GameLevelSerializerRecord;
    }

    void spGameLevelSerializer::BindForAnalysis(
        spGameLevel* const target,
        spStream* const input) noexcept
    {
        target_ = target;
        input_ = input;
    }

    spGameLevel* spGameLevelSerializer::GetTargetForAnalysis() const noexcept
    {
        return target_;
    }

    spStream* spGameLevelSerializer::GetInputForAnalysis() const noexcept
    {
        return input_;
    }
}
