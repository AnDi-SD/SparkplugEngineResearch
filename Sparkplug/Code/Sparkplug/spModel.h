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
        friend class spModelSerializer;
    public:
        static constexpr spClassID ClassID = 0x763277DB;
        static constexpr std::uint32_t NativeDefaultProjectionGroup = 3;
        // Compatibility alias: now independently confirmed on PC too.
        static constexpr std::uint32_t PS2DefaultProjectionGroup = 3;

        // Original PC constructor also initializes Renderable +28 to ARGB black.
        spModel() noexcept {SetField28ForAnalysis(0xff000000);}
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

        // PC479D20/479D40/479DA0 delegate to mesh18/2C/38/28. The sphere
        // getter does not gate on the separate bounds-valid byte.
        [[nodiscard]] const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept override;
        void GetBoundsForAnalysis(BoundsPosition& minimum,
                                  BoundsPosition& maximum) const noexcept override;
        [[nodiscard]] bool HasBoundsForAnalysis() const noexcept;

    protected:
        void InvalidateRuntimeModeForAnalysis() noexcept override;

    private:
        std::shared_ptr<spMesh> baseMesh_;
        std::uint32_t projectionGroup_ = NativeDefaultProjectionGroup;
    };
}
