#pragma once
#include "RuntimeContract.h"

namespace winxcoop::adapter
{
    // WinxCoop-owned visual smoothing, not recovered movement or physics.
    // Times are milliseconds on one local monotonic clock, never peer time.
    // One instance belongs to one qualified remote actor until Clear().
    class RemotePoseInterpolator final
    {
    public:
        static constexpr std::uint32_t DefaultDurationMilliseconds = 75;
        static constexpr std::uint32_t MaximumDurationMilliseconds = 1000;

        // Reset snaps to a qualified pose. Scale stays fixed while active;
        // Clear is required before establishing a different scale/cohort.
        bool Reset(const Transform& pose, std::uint64_t now) noexcept;
        // Replace a target starting at the pose currently sampled at now.
        // Duration must be1..1000ms; no extrapolation occurs after its end.
        bool SetTarget(const Transform& pose, std::uint64_t now,
            std::uint32_t duration = DefaultDurationMilliseconds) noexcept;
        // On failure neither output nor interpolator state changes. Repeated
        // timestamps are allowed; successful calls never move time backwards.
        bool Sample(std::uint64_t now, Transform& output) noexcept;
        bool Active() const noexcept { return active_; }
        void Clear() noexcept;

    private:
        struct Quaternion final { double w = 1, x = 0, y = 0, z = 0; };
        Transform start_{}, target_{};
        Quaternion startRotation_{}, targetRotation_{};
        std::uint64_t startTime_{}, lastTime_{};
        std::uint32_t duration_{};
        bool active_{};

        static bool Validate(const Transform& pose, Quaternion& rotation) noexcept;
        static bool Normalize(Quaternion& rotation) noexcept;
        static Quaternion Blend(Quaternion from, Quaternion to, double weight) noexcept;
        static void WriteMatrix(const Quaternion& rotation, float* matrix) noexcept;
        bool SameScale(const Transform& pose) const noexcept;
        void Evaluate(std::uint64_t now, Transform& pose, Quaternion& rotation) const noexcept;
    };
}
