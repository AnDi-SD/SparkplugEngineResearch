#pragma once

// PC-only native error leaf. Header/TU placement and portable names are
// analytical; the executable establishes class identity and behavior.
#include "../SparkBase/spErrorManager.h"

namespace sparkplug::reconstruction
{
    class spWindowsError : public spError
    {
    public:
        static constexpr spClassID ClassID = 0x1DE8113F;
        spWindowsError() noexcept = default;
        using spError::spError;
        ~spWindowsError() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::string DescribeForAnalysis() const override;
        [[nodiscard]] const char* ErrorNameForAnalysis() const noexcept override;
    };
}
