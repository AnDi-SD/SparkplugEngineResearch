#pragma once

// Class identity and the shipped PS2 operations are original-backed. Header
// path, portable namespace and analytical API names are reconstruction choices.
#include "spPS2InputDevice.h"

namespace sparkplug::reconstruction
{
    class spPS2Keyboard : public spPS2InputDevice
    {
    public:
        static constexpr spClassID ClassID = 0xA217BC14;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        // Original ready result is constant even before initialization. The
        // shipped leaves below do not consult keyboard state or poll a device.
        [[nodiscard]] bool ReadyForAnalysis() const noexcept { return true; }
        bool InitializeForAnalysis() noexcept;
        [[nodiscard]] bool PhysicalSlot1ForAnalysis(std::uint32_t) const noexcept { return false; }
        [[nodiscard]] bool PhysicalSlot2ForAnalysis(std::uint32_t) const noexcept { return false; }
        [[nodiscard]] std::uint32_t PhysicalSlot3ForAnalysis(std::uint32_t) const noexcept { return 0; }
        [[nodiscard]] std::int32_t PhysicalSlot4ForAnalysis(std::uint32_t) const noexcept { return 0; }
        void PhysicalSlot5ForAnalysis(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const noexcept {}
        [[nodiscard]] float PhysicalSlot6ForAnalysis(std::uint32_t) const noexcept { return 0.0f; }

        // Our adapter binds the original physical leaves to the common logical
        // routing facade. The returned callbacks borrow this object's lifetime.
        [[nodiscard]] QueriesForAnalysis GetQueriesForAnalysis() const;
    };
}
