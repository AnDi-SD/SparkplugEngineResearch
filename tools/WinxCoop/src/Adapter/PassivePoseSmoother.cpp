#include "PassivePoseSmoother.h"
#include <cstring>

namespace winxcoop::adapter
{
    bool PassivePoseSmoother::Reset(const Transform& pose, std::uint64_t now) noexcept
    {
        // The generic reset validates pose, scale and its current clock before
        // changing anything. Keep our policy history equally transactional.
        auto next = interpolation_;
        if (!next.Reset(pose, now)) return false;
        interpolation_ = next; lastTarget_ = pose; lastArrival_ = lastTime_ = now;
        duration_ = DefaultDurationMilliseconds;
        return true;
    }

    bool PassivePoseSmoother::SetTarget(const Transform& pose, std::uint64_t now) noexcept
    {
        if (!Active() || now < lastTime_) return false;
        const auto gap = now - lastArrival_;
        double distanceSquared = 0;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const double delta = static_cast<double>(pose.position[axis]) - lastTarget_.position[axis];
            distanceSquared += delta * delta;
        }
        const bool discontinuity = gap >= StaleGapMilliseconds ||
            distanceSquared >= DiscontinuityDistance * DiscontinuityDistance;
        auto duration = duration_;
        if (discontinuity) duration = DefaultDurationMilliseconds;
        else if (gap)
        {
            const auto interval = gap < MinimumDurationMilliseconds ? MinimumDurationMilliseconds :
                gap > MaximumDurationMilliseconds ? MaximumDurationMilliseconds : static_cast<std::uint32_t>(gap);
            // Bounded 3:1 arrival-cadence average. Duplicate pose arrivals also
            // contribute freshness/cadence, but do not restart the segment.
            duration = (3 * duration + interval + 2) / 4;
        }

        auto next = interpolation_;
        if (std::memcmp(&pose, &lastTarget_, sizeof(pose)) == 0)
        {
            // Validate even a duplicate through the same generic admission
            // path. Discard this validation-only reset: committing it would
            // snap, while SetTarget would continually postpone the endpoint.
            auto validation = next;
            if (!validation.Reset(pose, now)) return false;
            Transform current{};
            if (!next.Sample(now, current)) return false;
        }
        else if (discontinuity)
        {
            // A large correction or resumed stream establishes one display
            // pose instead of sweeping through unobserved level geometry.
            if (!next.Reset(pose, now)) return false;
        }
        else if (!next.SetTarget(pose, now, duration)) return false;

        interpolation_ = next; lastTarget_ = pose; lastArrival_ = lastTime_ = now; duration_ = duration;
        return true;
    }

    bool PassivePoseSmoother::Sample(std::uint64_t now, Transform& output) noexcept
    {
        if (!interpolation_.Sample(now, output)) return false;
        lastTime_ = now;
        return true;
    }

    void PassivePoseSmoother::Clear() noexcept { *this = PassivePoseSmoother{}; }
}
