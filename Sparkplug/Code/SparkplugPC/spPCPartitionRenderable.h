#pragma once
// Inferred source path. PC4CD950 allocation8C,4CDA10 clone uses Base copy only.
// CPU resource state only; original device draw/legacy OS calls are not used.
#include "Code/Sparkplug/spPartitionRenderable.h"
namespace sparkplug::reconstruction
{
    class spPCPartitionRenderable final : public spPartitionRenderable
    {
    public:
        static constexpr spClassID ClassID=0x9CBB56A2;
        spPCPartitionRenderable() noexcept=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    };
}
