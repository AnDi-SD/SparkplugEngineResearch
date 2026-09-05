#pragma once

// Inferred declaration path. Native RTTI, construction and serialization
// establish the class and fields, but no original source path survives.

#include "spRenderable.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spMesh;

    class spModel : public spRenderable
    {
    public:
        static constexpr spClassID ClassID = 0x763277DB;
        static constexpr std::uint32_t PS2DefaultProjectionGroup = 3;

        spModel() noexcept = default;
        ~spModel() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(
            spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        void SetBaseMeshForAnalysis(std::shared_ptr<spMesh> mesh) noexcept;
        [[nodiscard]] const std::shared_ptr<spMesh>&
            GetBaseMeshForAnalysis() const noexcept;
        void SetProjectionGroupForAnalysis(std::uint32_t group) noexcept;
        [[nodiscard]] std::uint32_t GetProjectionGroupForAnalysis() const noexcept;

    private:
        std::shared_ptr<spMesh> baseMesh_;
        // The portable choice follows the only constructor whose body is
        // visible. PC default remains an explicit open question.
        std::uint32_t projectionGroup_ = PS2DefaultProjectionGroup;
    };
}
