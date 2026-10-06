#pragma once

// Native class name/identity and own integer behavior. Path, names and host
// representation are analytical; this class does not claim the 32-bit ABI.
#include "../SparkBase/spBaseObject.h"
#include <functional>

namespace sparkplug::reconstruction
{
    class spTimer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x149C778B;
        struct StateForAnalysis
        {
            std::uint8_t active = 0;
            std::uint32_t accumulated = 0;
            std::uint32_t startedAt = 0;
            std::uint8_t limited = 0;
            std::uint32_t limit = 0;
        };
        // Foreign PC WINMM.timeGetTime / PS2 001E7980 boundary. Each call is
        // a distinct sample. No host OS clock or PS2 wrapper is implemented.
        using ClockSourceForAnalysis = std::function<std::uint32_t()>;

        spTimer() noexcept = default;
        spTimer(std::uint8_t startImmediately, const ClockSourceForAnalysis&);
        ~spTimer() override = default;
        spTimer(const spTimer&) = delete;
        spTimer& operator=(const spTimer&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Copy is the inherited successful root no-op, including self-copy.

        [[nodiscard]] bool StartForAnalysis(const ClockSourceForAnalysis&,
                                            std::string* error = nullptr);
        [[nodiscard]] bool StopForAnalysis(const ClockSourceForAnalysis&,
                                           std::string* error = nullptr);
        [[nodiscard]] bool ResetForAnalysis(const ClockSourceForAnalysis&,
                                            std::string* error = nullptr);
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept
        { return state_; }
        // Literal fixture setup; original setter declarations remain unknown.
        void SetStateForAnalysis(const StateForAnalysis& state) noexcept { state_ = state; }

    private:
        StateForAnalysis state_{};
    };
}
