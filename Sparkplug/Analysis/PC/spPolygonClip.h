#pragma once

// Original PC491AA0 geometric slice. Native value-class name is NOT recovered;
// this is an analysis utility, not an invented engine class or full ring/pool.
// Scratch metadata/alias move semantics and allocation failure are excluded.
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace sparkplug::evidence::pc::polygon_clip
{
    using Vector3 = std::array<float, 3>;
    using Plane = std::array<float, 4>;
    inline double Distance(const Plane& plane, const Vector3& point) noexcept
    {
        return (double(point[2]) * plane[2] + double(point[1]) * plane[1]) +
               double(plane[0]) * point[0] - plane[3];
    }

    // Copies source before changing destination, supporting host aliasing.
    // Native stack has128 classification/distance slots including closing
    // duplicate. Host rejects >127 before mutation; native lacks this check.
    // NaN classification follows original unordered branches; no finite guard.
    [[nodiscard]] inline bool ClipForAnalysis(const std::vector<Vector3>& source,
                                              const Plane& plane, bool keepCoplanar,
                                              std::vector<Vector3>& destination,
                                              float epsilon = 0.001F)
    {
        if (source.size() > 127)
            return false;
        const auto points = source;
        std::vector<unsigned> sides;
        std::vector<double> distances;
        std::size_t positive = 0, negative = 0;
        for (const auto& point : points)
        {
            const double distance = Distance(plane, point);
            const unsigned side = distance < -double(epsilon)  ? 1
                                  : distance > double(epsilon) ? 0
                                                               : 2;
            sides.push_back(side);
            distances.push_back(distance);
            positive += side == 0;
            negative += side == 1;
        }
        if (!positive)
        {
            if (keepCoplanar && !negative)
            {
                destination = points;
                return true; // includes empty source when keepCoplanar=true
            }
            destination.clear();
            return false;
        }
        if (!negative)
        {
            destination = points;
            return true;
        }
        std::vector<Vector3> result;
        result.reserve(points.size() * 2);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const auto next = (i + 1) % points.size();
            if (sides[i] == 2)
            {
                result.push_back(points[i]);
                continue;
            }
            if (sides[i] == 0)
                result.push_back(points[i]);
            if (sides[next] == 2 || sides[next] == sides[i])
                continue;
            const double t = distances[i] / (distances[i] - distances[next]);
            Vector3 intersection{};
            for (std::size_t axis = 0; axis < 3; ++axis)
                intersection[axis] = static_cast<float>(
                    (double(points[next][axis]) - points[i][axis]) * t + points[i][axis]);
            result.push_back(intersection);
        }
        // Original491DC0..491E24 never advances the point pointer. Preserve
        // repeated FIRST-point correction, not a conventional all-point pass.
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            auto& point = result.front();
            const double distance = Distance(plane, point);
            if (distance <= -double(epsilon))
            {
                // Native Y/Z deltas are float-stored before addition; X is
                // kept in x87 until its final sum/store.
                const float dy = static_cast<float>(-distance * plane[1]);
                const float dz = static_cast<float>(-distance * plane[2]);
                point[0] = static_cast<float>(-distance * plane[0] + point[0]);
                point[1] = static_cast<float>(double(dy) + point[1]);
                point[2] = static_cast<float>(double(dz) + point[2]);
            }
        }
        destination = std::move(result);
        return true;
    }
} // namespace sparkplug::evidence::pc::polygon_clip
