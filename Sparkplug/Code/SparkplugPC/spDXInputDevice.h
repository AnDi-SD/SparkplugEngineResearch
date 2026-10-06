#pragma once
#include "../Sparkplug/spInputDevice.h"

namespace sparkplug::reconstruction
{
    class spDXInputDevice : public spInputDevice
    {
    public:
        static constexpr spClassID ClassID = 0x2C2F3993;
        using ForeignReferencesForAnalysis = sparkplug::analysis::host::spDXInputDeviceReferencesForAnalysis;
        struct StateForAnalysis final
        {
            std::uint8_t field44 = 0;
            std::uint8_t acquired45 = 0;
            std::uint32_t field48 = 0;
            std::uintptr_t inputInterface4C = 0;
            std::uintptr_t device50 = 0;
            std::uint32_t field80 = 3;
            std::uint8_t field84 = 1;
        };
        // Boundary must outlive this object; the original constructor obtains
        // an already valid external input interface and retains it once.
        explicit spDXInputDevice(ForeignReferencesForAnalysis&);
        ~spDXInputDevice() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        void SetStateForAnalysis(const StateForAnalysis& state) noexcept { state_ = state; }
    protected:
        // Recovered device startup passes the live +50 cell as a COM out
        // parameter and re-reads it after callbacks. This does not expose ABI.
        [[nodiscard]] StateForAnalysis& MutableStateForAnalysis() noexcept { return state_; }
    private:
        ForeignReferencesForAnalysis& references_;
        StateForAnalysis state_{};
    };
}
