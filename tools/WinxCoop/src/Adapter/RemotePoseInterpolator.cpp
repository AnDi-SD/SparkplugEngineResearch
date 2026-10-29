#include "RemotePoseInterpolator.h"
#include <cmath>
#include <limits>

namespace winxcoop::adapter
{
    namespace
    {
        // Admit bounded float roundoff; interpolated output is rebuilt from
        // a normalized quaternion rather than interpolating matrix entries.
        constexpr double MatrixTolerance = 0.02;
        bool Finite(float value) noexcept { return std::isfinite(static_cast<double>(value)); }
        double ClampUnit(double value) noexcept { return value < -1 ? -1 : value > 1 ? 1 : value; }
    }

    bool RemotePoseInterpolator::Normalize(Quaternion& q) noexcept
    {
        const double squared = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
        if (!std::isfinite(squared) || squared < 1e-24) return false;
        const double inverse = 1 / std::sqrt(squared);
        q.w *= inverse; q.x *= inverse; q.y *= inverse; q.z *= inverse;
        return true;
    }

    bool RemotePoseInterpolator::Validate(const Transform& pose, Quaternion& q) noexcept
    {
        for (unsigned i = 0; i < 3; ++i)
            if (!Finite(pose.position[i]) || !Finite(pose.scale[i]) || pose.scale[i] <= 0) return false;
        for (float value : pose.orientation) if (!Finite(value)) return false;
        const auto* m = pose.orientation;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned other = row; other < 3; ++other)
            {
                double dot = 0;
                for (unsigned c = 0; c < 3; ++c) dot += static_cast<double>(m[row*3+c])*m[other*3+c];
                if (std::abs(dot - (row == other ? 1.0 : 0.0)) > MatrixTolerance) return false;
            }
        const double determinant = static_cast<double>(m[0])*(static_cast<double>(m[4])*m[8] - static_cast<double>(m[5])*m[7])
            - static_cast<double>(m[1])*(static_cast<double>(m[3])*m[8] - static_cast<double>(m[5])*m[6])
            + static_cast<double>(m[2])*(static_cast<double>(m[3])*m[7] - static_cast<double>(m[4])*m[6]);
        if (std::abs(determinant - 1) > MatrixTolerance) return false;

        const double trace = static_cast<double>(m[0]) + m[4] + m[8];
        double s;
        if (trace > 0)
        {
            s = 2 * std::sqrt(trace + 1);
            q = {s/4, (static_cast<double>(m[7])-m[5])/s,
                (static_cast<double>(m[2])-m[6])/s, (static_cast<double>(m[3])-m[1])/s};
        }
        else if (m[0] > m[4] && m[0] > m[8])
        {
            s = 2 * std::sqrt(1 + static_cast<double>(m[0]) - m[4] - m[8]);
            q = {(static_cast<double>(m[7])-m[5])/s, s/4,
                (static_cast<double>(m[1])+m[3])/s, (static_cast<double>(m[2])+m[6])/s};
        }
        else if (m[4] > m[8])
        {
            s = 2 * std::sqrt(1 + static_cast<double>(m[4]) - m[0] - m[8]);
            q = {(static_cast<double>(m[2])-m[6])/s,
                (static_cast<double>(m[1])+m[3])/s, s/4, (static_cast<double>(m[5])+m[7])/s};
        }
        else
        {
            s = 2 * std::sqrt(1 + static_cast<double>(m[8]) - m[0] - m[4]);
            q = {(static_cast<double>(m[3])-m[1])/s,
                (static_cast<double>(m[2])+m[6])/s, (static_cast<double>(m[5])+m[7])/s, s/4};
        }
        return Normalize(q);
    }

    RemotePoseInterpolator::Quaternion RemotePoseInterpolator::Blend(Quaternion a, Quaternion b, double weight) noexcept
    {
        double dot = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
        if (dot < 0)
        {
            b.w = -b.w; b.x = -b.x; b.y = -b.y; b.z = -b.z;
            dot = -dot;
        }
        dot = ClampUnit(dot);
        double left = 1-weight, right = weight;
        if (dot < 0.9995)
        {
            const double angle = std::acos(dot), sine = std::sin(angle);
            left = std::sin((1-weight)*angle)/sine;
            right = std::sin(weight*angle)/sine;
        }
        Quaternion result{left*a.w+right*b.w, left*a.x+right*b.x,
            left*a.y+right*b.y, left*a.z+right*b.z};
        // Unit endpoints with their shortest hemisphere selected guarantee
        // a finite, nonzero blend for weight in [0,1].
        Normalize(result);
        return result;
    }

    void RemotePoseInterpolator::WriteMatrix(const Quaternion& q, float* m) noexcept
    {
        m[0] = static_cast<float>(1-2*(q.y*q.y+q.z*q.z));
        m[1] = static_cast<float>(2*(q.x*q.y-q.z*q.w));
        m[2] = static_cast<float>(2*(q.x*q.z+q.y*q.w));
        m[3] = static_cast<float>(2*(q.x*q.y+q.z*q.w));
        m[4] = static_cast<float>(1-2*(q.x*q.x+q.z*q.z));
        m[5] = static_cast<float>(2*(q.y*q.z-q.x*q.w));
        m[6] = static_cast<float>(2*(q.x*q.z-q.y*q.w));
        m[7] = static_cast<float>(2*(q.y*q.z+q.x*q.w));
        m[8] = static_cast<float>(1-2*(q.x*q.x+q.y*q.y));
    }

    bool RemotePoseInterpolator::SameScale(const Transform& pose) const noexcept
    {
        for (unsigned i = 0; i < 3; ++i) if (pose.scale[i] != start_.scale[i]) return false;
        return true;
    }

    void RemotePoseInterpolator::Evaluate(std::uint64_t now, Transform& pose, Quaternion& rotation) const noexcept
    {
        const std::uint64_t elapsed = now-startTime_;
        const double weight = !duration_ || elapsed >= duration_ ? 1 : static_cast<double>(elapsed)/duration_;
        pose = start_;
        for (unsigned i = 0; i < 3; ++i)
            pose.position[i] = static_cast<float>((1-weight)*static_cast<double>(start_.position[i]) + weight*target_.position[i]);
        rotation = Blend(startRotation_, targetRotation_, weight);
        WriteMatrix(rotation, pose.orientation);
    }

    bool RemotePoseInterpolator::Reset(const Transform& pose, std::uint64_t now) noexcept
    {
        Quaternion rotation;
        if ((active_ && (now < lastTime_ || !SameScale(pose))) || !Validate(pose, rotation)) return false;
        start_ = target_ = pose; startRotation_ = targetRotation_ = rotation;
        startTime_ = lastTime_ = now; duration_ = 0; active_ = true;
        return true;
    }

    bool RemotePoseInterpolator::SetTarget(const Transform& pose, std::uint64_t now, std::uint32_t duration) noexcept
    {
        Quaternion rotation;
        if (!active_ || now < lastTime_ || !duration || duration > MaximumDurationMilliseconds ||
            now > (std::numeric_limits<std::uint64_t>::max)()-duration || !SameScale(pose) || !Validate(pose, rotation)) return false;
        Transform current;
        Quaternion currentRotation;
        Evaluate(now, current, currentRotation);
        start_ = current; target_ = pose; startRotation_ = currentRotation; targetRotation_ = rotation;
        startTime_ = lastTime_ = now; duration_ = duration;
        return true;
    }

    bool RemotePoseInterpolator::Sample(std::uint64_t now, Transform& output) noexcept
    {
        if (!active_ || now < lastTime_) return false;
        Transform pose;
        Quaternion rotation;
        Evaluate(now, pose, rotation);
        output = pose; lastTime_ = now;
        return true;
    }

    void RemotePoseInterpolator::Clear() noexcept { *this = RemotePoseInterpolator{}; }
}
