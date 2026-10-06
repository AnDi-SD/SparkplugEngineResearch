#pragma once

// Recovered PC/PS2 error leaf. File placement and C++ API are analytical;
// no original source-file spelling for this class has been established.
#include "spErrorManager.h"

namespace sparkplug::reconstruction
{
    class spStreamError : public spError
    {
    public:
        static constexpr spClassID ClassID = 0x4D2842F6;
        spStreamError() noexcept = default;
        using spError::spError;
        ~spStreamError() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::string DescribeForAnalysis() const override;
        [[nodiscard]] const char* ErrorNameForAnalysis() const noexcept override;
    };
}
