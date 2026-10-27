#pragma once
#include "Code/Sparkplug/spProjectionManager.h"

namespace sparkplug::reconstruction
{
    class spPCProjectionManager : public spProjectionManager
    {
    public:
        static constexpr spClassID ClassID=0xE10D6FC2;
        spPCProjectionManager()=default;
        ~spPCProjectionManager() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    };
}
