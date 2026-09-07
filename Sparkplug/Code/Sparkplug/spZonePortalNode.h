#pragma once

// Original PC class; source paths inferred, serializer TU separately proven.
// Node world/Enabled operations are inherited and do not modify portal geometry
// or Open. Own vector is borrowed and allows repeated pointers, including null
// at the raw append481930 layer (the original reader rejects null beforehand).
#include "spNode.h"
#include "spZonePortal.h"

namespace sparkplug::reconstruction
{
    class spZonePortalNode : public spNode
    {
      public:
        static constexpr spClassID ClassID = 0xABB5AB2C;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        // Host4096 limit only; no retain, de-duplication or implicit pair policy.
        [[nodiscard]] bool AppendPortalForAnalysis(spZonePortal* portal);
        [[nodiscard]] const std::vector<spZonePortal*>& GetPortalsForAnalysis() const noexcept;
        // Original name from serializer diagnostic, host null-on-bounds guard.
        [[nodiscard]] spZonePortal* GetZonePortal(std::size_t index) const noexcept;

      private:
        std::vector<spZonePortal*> portals_;
    };
} // namespace sparkplug::reconstruction
