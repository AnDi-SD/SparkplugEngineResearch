#pragma once
#include "RemotePoseInterpolator.h"

namespace winxcoop::adapter
{
    // Passive-display policy over the shared interpolation math. It never
    // predicts movement or writes the locally controlled character. Times are
    // destination monotonic milliseconds, not peer ticks or synchronized time.
    class PassivePoseSmoother final
    {
    public:
        static constexpr std::uint32_t DefaultDurationMilliseconds = 50;
        static constexpr std::uint32_t MinimumDurationMilliseconds = 20;
        static constexpr std::uint32_t MaximumDurationMilliseconds = 100;
        static constexpr std::uint64_t StaleGapMilliseconds = 500;
        static constexpr double DiscontinuityDistance = 512;

        // Clear is required before a different scale/cohort or local clock.
        bool Reset(const Transform& pose, std::uint64_t now) noexcept;
        bool SetTarget(const Transform& pose, std::uint64_t now) noexcept;
        bool Sample(std::uint64_t now, Transform& output) noexcept;
        bool Active() const noexcept { return interpolation_.Active(); }
        void Clear() noexcept;

    private:
        RemotePoseInterpolator interpolation_;
        Transform lastTarget_{};
        std::uint64_t lastArrival_{}, lastTime_{};
        std::uint32_t duration_ = DefaultDurationMilliseconds;
    };
}
