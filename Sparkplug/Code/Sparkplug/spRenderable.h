#pragma once

// Inferred declaration path. The native class name and its position between
// spNamedObject and spModel are present in both shipped executables; no
// original header/translation-unit path has been recovered.

#include "Code/SparkBase/spBaseObject.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spRenderable : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x4FDA4542;

        spRenderable() noexcept = default;
        ~spRenderable() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native spRenderable has no RTTI factory and its clone slot is null.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(
            spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable ownership facade. Concrete spMaterialData/spFog now exist,
        // but the common native relationship accepts their abstract families;
        // keeping spBaseObject here avoids inventing an unproved C++ signature.
        void SetMaterialForAnalysis(std::shared_ptr<spBaseObject> material) noexcept;
        void SetFogForAnalysis(std::shared_ptr<spBaseObject> fog) noexcept;
        [[nodiscard]] const std::shared_ptr<spBaseObject>&
            GetMaterialForAnalysis() const noexcept;
        [[nodiscard]] const std::shared_ptr<spBaseObject>&
            GetFogForAnalysis() const noexcept;

        void SetAlphaSortEnabledForAnalysis(bool enabled) noexcept;
        [[nodiscard]] bool IsAlphaSortEnabledForAnalysis() const noexcept;
        void SetPriorityForAnalysis(std::uint32_t priority) noexcept;
        [[nodiscard]] std::uint32_t GetPriorityForAnalysis() const noexcept;

    protected:
        void InvalidateRuntimeModeForAnalysis() noexcept;
        [[nodiscard]] std::uint32_t GetRuntimeModeForAnalysis() const noexcept;

    private:
        std::uint32_t runtimeMode_ = 0;
        bool alphaSortEnabled_ = true;
        std::uint32_t priority_ = 0;
        std::shared_ptr<spBaseObject> material_;
        std::shared_ptr<spBaseObject> fog_;
    };
}
