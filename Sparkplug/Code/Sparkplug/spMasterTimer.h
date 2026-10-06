#pragma once

// Original PC/PS2 timer subclass. API spelling, path and portable singleton
// representation are analytical, not the native secondary-interface ABI.
#include "spTimer.h"

namespace sparkplug::reconstruction
{
    class spMasterTimer : public spTimer
    {
    public:
        static constexpr spClassID ClassID = 0x287E268B;
        spMasterTimer() noexcept;
        ~spMasterTimer() override;
        spMasterTimer(const spMasterTimer&) = delete;
        spMasterTimer& operator=(const spMasterTimer&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Literal native global pointer read. Lazy ownership/acquisition and
        // the native secondary vtable are not supplied by this convenience API.
        [[nodiscard]] static spMasterTimer* GetInstanceForAnalysis() noexcept;
    private:
        static spMasterTimer* instance_;
    };
}
