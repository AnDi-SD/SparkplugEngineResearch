#pragma once

// Analytical value adapter for PC 0x479290/0x479830, NOT a newly invented
// original engine class, native ABI, complete SAN loader or object directory.
// It accepts decoded key records, prepares an immutable snapshot and supplies
// the existing spTransformTrackEval sampling seam with executable-backed math.
#include "spAnimationMath.h"
#include "../../Code/Sparkplug/spTransformTrackEval.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace sparkplug::evidence::pc::animation_keys
{
    struct KeyDataForAnalysis
    {
        unsigned representation = 1; // original numeric values 1/2/3/4
        std::vector<float> times;
        std::vector<float> values;
    };
    // role 0/1/2 = position/rotation/scale. Packed representations use axis 0;
    // scalar representations use three independently timed axis descriptors.
    using TrackDataForAnalysis = std::array<std::array<std::optional<KeyDataForAnalysis>, 3>, 3>;
    using KeyCache = reconstruction::spTransformTrackEval::KeyCacheForAnalysis;
    using Sample = reconstruction::spTransformEval::SampleForAnalysis;

    // PC493160 (scalar) and493290 (vector) use different x87 spill sites.
    // Even vector X/Y/Z do not share one rounding schedule. Wider arithmetic
    // alone is insufficient: preserve each observed float store explicitly.
    inline std::array<float, 2> CubicCoefficientsForAnalysis(
        float current, float next, float outgoing, float incoming,
        std::size_t dimensions, std::size_t axis)
    {
        const double delta = double(next) - current;
        const double out = outgoing, in = incoming;
        if (dimensions == 1)
            return {static_cast<float>(3 * delta - (2 * out + in)),
                    static_cast<float>((in + out) - 2 * delta)};
        const float deltaFloat = static_cast<float>(delta);
        const float triple = static_cast<float>(3 * (axis == 2 ? double(deltaFloat) : delta));
        const double tangent = axis == 2
            ? double(static_cast<float>(2 * out)) + in
            : double(static_cast<float>(2 * out + in));
        const double doubled = axis == 2 ? 2 * double(deltaFloat)
            : double(static_cast<float>(2 * delta));
        const double sum = axis == 0 ? in + out : double(static_cast<float>(in + out));
        return {static_cast<float>(double(triple) - tangent),
                static_cast<float>(sum - doubled)};
    }

    class PreparedTrackForAnalysis
    {
      public:
        [[nodiscard]] static std::optional<PreparedTrackForAnalysis> Create(
            TrackDataForAnalysis data)
        {
            for (std::size_t role = 0; role < 3; ++role)
            {
                if (!data[role][0])
                    continue;
                const bool scalar = data[role][0]->representation >= 3;
                for (std::size_t axis = 0; axis < (scalar ? 3U : 1U); ++axis)
                {
                    auto& slot = data[role][axis];
                    if (!slot)
                        return std::nullopt; // native assumes a complete scalar triple
                    auto& key = *slot;
                    const auto rep = key.representation;
                    if (rep < 1 || rep > 4 || (rep >= 3) != scalar)
                        return std::nullopt;
                    const auto count = key.times.size(), stride = Stride(rep, role);
                    if (count > 100000 || key.values.size() != count * stride)
                        return std::nullopt;
                    if (!std::is_sorted(key.times.begin(), key.times.end()))
                        return std::nullopt;
                    for (auto value : key.times)
                        if (!std::isfinite(value))
                            return std::nullopt;
                    for (auto value : key.values)
                        if (!std::isfinite(value))
                            return std::nullopt;
                    // Explicit host guards for unsafe empty scalar/precompute
                    // branches. Packed linear empty channels are native-valid.
                    if (!count && (scalar || rep == 2))
                        return std::nullopt;
                    if (rep == 2 && role == 1)
                    {
                        for (std::size_t i = 0; i < count; ++i)
                        {
                            const auto current = QuaternionAt(key, i * 8);
                            const auto control =
                                count == 1 ? current
                                           : animation_math::Control(
                                                 QuaternionAt(key, (i ? i - 1 : 0) * 8), current,
                                                 QuaternionAt(key, std::min(i + 1, count - 1) * 8));
                            for (std::size_t c = 0; c < 4; ++c)
                                key.values[i * 8 + 4 + c] = control[c];
                        }
                    }
                    else if (rep == 2 || rep == 4)
                    {
                        const std::size_t dimensions = rep == 4 ? 1 : 3;
                        for (std::size_t i = 0; i + 1 < count; ++i)
                            for (std::size_t c = 0; c < dimensions; ++c)
                            {
                                const auto a = i * stride + c, b = (i + 1) * stride + c;
                                const float outgoing = key.values[a + 2 * dimensions],
                                            incoming = key.values[b + dimensions];
                                const auto coefficients = CubicCoefficientsForAnalysis(
                                    key.values[a], key.values[b], outgoing, incoming, dimensions, c);
                                key.values[a + 3 * dimensions] = coefficients[0];
                                key.values[a + 4 * dimensions] = coefficients[1];
                            }
                    }
                    for (auto value : key.values)
                        if (!std::isfinite(value))
                            return std::nullopt;
                }
            }
            return PreparedTrackForAnalysis(std::move(data));
        }

        // PC spAnimTrack vslot +0x1c: maximum final time, starting at zero.
        [[nodiscard]] float GetDurationForAnalysis() const noexcept
        {
            float duration = 0;
            for (const auto& role : data_)
            {
                if (!role[0])
                    continue;
                const std::size_t axes = role[0]->representation >= 3 ? 3 : 1;
                for (std::size_t axis = 0; axis < axes; ++axis)
                    if (!role[axis]->times.empty())
                        duration = std::max(duration, role[axis]->times.back());
            }
            return duration;
        }

        [[nodiscard]] Sample Evaluate(float time, KeyCache& cache) const
        {
            Sample result;
            if (!std::isfinite(time))
                return result; // host-only invalid-input guard
            std::array<int*, 3> caches{cache.position.data(), cache.rotation.data(),
                                       cache.scale.data()};
            for (std::size_t role = 0; role < 3; ++role)
            {
                if (!data_[role][0])
                    continue;
                const auto& first = *data_[role][0];
                if (first.times.empty())
                    continue;
                bool& valid = role == 0   ? result.hasPosition
                              : role == 1 ? result.hasRotation
                                          : result.hasScale;
                valid = true;
                if (first.representation >= 3)
                {
                    std::array<float, 3> value{};
                    for (std::size_t axis = 0; axis < 3; ++axis)
                        value[axis] =
                            VectorSample(*data_[role][axis], time, caches[role][axis], 1)[0];
                    if (role == 1)
                    {
                        result.rotation = {0, 0, 0, 1};
                        for (std::size_t axis = 0; axis < 3; ++axis)
                        {
                            animation_math::Quaternion q{0, 0, 0, std::cos(value[axis] * 0.5F)};
                            q[axis] = std::sin(value[axis] * 0.5F);
                            result.rotation = animation_math::Multiply(q, result.rotation);
                        }
                    }
                    else if (role == 0)
                        result.position = value;
                    else
                        result.scale = value;
                }
                else if (role == 1)
                {
                    const auto [index, factor] = Interval(first, time, caches[role][0]);
                    const auto stride = Stride(first.representation, role);
                    const auto next = std::min(index + 1, first.times.size() - 1);
                    const auto a = QuaternionAt(first, index * stride),
                               b = QuaternionAt(first, next * stride);
                    // Native reads a neighbour even for a one-key quaternion;
                    // finite adjacent data yields the same first key at t=0.
                    // Host uses a bounded repeated endpoint, not an OOB read.
                    if (first.times.size() == 1)
                        result.rotation = a;
                    else if (first.representation == 1)
                        result.rotation = animation_math::Interpolate(a, b, factor);
                    else
                        result.rotation = animation_math::Interpolate(
                            animation_math::Interpolate(a, b, factor),
                            animation_math::Interpolate(QuaternionAt(first, index * stride + 4),
                                                        QuaternionAt(first, next * stride + 4),
                                                        factor),
                            2 * factor * (1 - factor));
                }
                else
                {
                    const auto value = VectorSample(first, time, caches[role][0], 3);
                    if (role == 0)
                        result.position = value;
                    else
                        result.scale = value;
                }
            }
            return result;
        }
        [[nodiscard]] const TrackDataForAnalysis& Data() const noexcept
        {
            return data_;
        }

      private:
        explicit PreparedTrackForAnalysis(TrackDataForAnalysis data) : data_(std::move(data))
        {
        }
        static std::size_t Stride(unsigned rep, std::size_t role)
        {
            if (rep == 3)
                return 1;
            if (rep == 4)
                return 5;
            return role == 1 ? (rep == 1 ? 4 : 8) : (rep == 1 ? 3 : 15);
        }
        static animation_math::Quaternion QuaternionAt(const KeyDataForAnalysis& key, std::size_t i)
        {
            return {key.values[i], key.values[i + 1], key.values[i + 2], key.values[i + 3]};
        }
        static std::pair<std::size_t, float> Interval(const KeyDataForAnalysis& key, float time,
                                                      int& cache)
        {
            const auto count = key.times.size();
            std::size_t index = cache < 0 || static_cast<std::size_t>(cache) >= count
                                    ? 0
                                    : static_cast<std::size_t>(cache);
            while (index && time < key.times[index])
                --index;
            while (index + 1 < count && time >= key.times[index + 1])
                ++index;
            float factor = 0;
            if (index == count - 1)
            {
                if (index > 1)
                {
                    --index;
                    factor = 1;
                }
                else
                    index = 0; // native two-key endpoint behavior, intentionally retained
            }
            else if (time >= key.times[index])
                factor = (time - key.times[index]) / (key.times[index + 1] - key.times[index]);
            cache = static_cast<int>(index);
            return {index, factor};
        }
        static std::array<float, 3> VectorSample(const KeyDataForAnalysis& key, float time,
                                                 int& cache, std::size_t dimensions)
        {
            const auto [index, factor] = Interval(key, time, cache);
            const bool cubic = key.representation == 2 || key.representation == 4;
            const auto stride = dimensions * (cubic ? 5 : 1), a = index * stride,
                       b = std::min(index + 1, key.times.size() - 1) * stride;
            std::array<float, 3> result{};
            for (std::size_t c = 0; c < dimensions; ++c)
                result[c] =
                    cubic ? key.values[a + c] +
                                factor * (key.values[a + 2 * dimensions + c] +
                                          factor * (key.values[a + 3 * dimensions + c] +
                                                    factor * key.values[a + 4 * dimensions + c]))
                          : key.values[a + c] * (1 - factor) + key.values[b + c] * factor;
            return result;
        }
        TrackDataForAnalysis data_;
    };
    inline reconstruction::spTransformTrackEval::TrackSamplerForAnalysis MakeSampler(
        std::shared_ptr<const PreparedTrackForAnalysis> track)
    {
        return [track = std::move(track)](float time, KeyCache& cache) {
            return track ? track->Evaluate(time, cache) : Sample{};
        };
    }
} // namespace sparkplug::evidence::pc::animation_keys
