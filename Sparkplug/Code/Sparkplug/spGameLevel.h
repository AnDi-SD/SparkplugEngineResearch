#pragma once

// Inferred declaration path. Both shipped binaries prove the class name and
// layout; only spGameLevelSerializer.cpp survives as an exact source path.

#include "../SparkBase/spBaseObject.h"
#include "spTemplateInstance.h"

#include <cstddef>
#include <list>
#include <memory>

namespace sparkplug::reconstruction
{
    class spGameLevel final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x4A45115B;

        spGameLevel() noexcept;
        ~spGameLevel() override;

        spGameLevel(const spGameLevel&) = delete;
        spGameLevel& operator=(const spGameLevel&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool AddInstanceForAnalysis(
            std::unique_ptr<spTemplateInstance> instance);
        [[nodiscard]] std::unique_ptr<spTemplateInstance>
            RemoveInstanceForAnalysis(spTemplateInstance& instance) noexcept;
        void ClearInstancesForAnalysis() noexcept;
        [[nodiscard]] std::size_t GetInstanceCountForAnalysis() const noexcept;

    private:
        std::list<std::unique_ptr<spTemplateInstance>> instances_;
        std::shared_ptr<spBaseObject> field20_;
        std::shared_ptr<spBaseObject> field24_;
        std::shared_ptr<spBaseObject> field28_;
    };
}
