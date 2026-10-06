#pragma once
#include "../Sparkplug/spInputDevice.h"

namespace sparkplug::reconstruction
{
    class spPS2InputDevice : public spInputDevice
    {
    public:
        static constexpr spClassID ClassID = 0x48B004B2;
        spPS2InputDevice() = default;
        ~spPS2InputDevice() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::uint8_t GetField44ForAnalysis() const noexcept { return field44_; }
        void SetField44ForAnalysis(std::uint8_t value) noexcept { field44_ = value; }
    private:
        std::uint8_t field44_ = 0;
    };
}
