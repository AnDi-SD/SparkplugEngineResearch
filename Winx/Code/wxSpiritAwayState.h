#pragma once

#include "wxCharacterState.h"

namespace winx::reconstruction
{
    // Every native behavior slot is exactly the wxCharacterState base slot.
    class wxSpiritAwayState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0xF3333312;
        wxSpiritAwayState() noexcept = default;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
    };
}
