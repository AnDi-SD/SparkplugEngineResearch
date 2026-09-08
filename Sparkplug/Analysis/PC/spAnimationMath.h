#pragma once

// Analytical names for PC 0x004648C0, 0x004647F0 and 0x00464CB0.
// Finite-input behavioral reconstruction; std math need not be bit-identical
// to the original x87/CRT implementation. No implicit normalization is added.
#include <array>
#include <cmath>

namespace sparkplug::evidence::pc::animation_math
{
    using Quaternion = std::array<float, 4>;
    using Matrix3 = std::array<float, 9>;
    inline Quaternion Multiply(const Quaternion& a, const Quaternion& b)
    {
        // Original x87 rounds on component stores, not each arithmetic step.
        // Keep wider intermediates; this is not quaternion normalization.
        const double x=a[0], y=a[1], z=a[2], w=a[3];
        const double u=b[0], v=b[1], s=b[2], t=b[3];
        return {static_cast<float>(w*u+x*t+y*s-z*v),
                static_cast<float>(w*v-x*s+y*t+z*u),
                static_cast<float>(w*s+x*v-y*u+z*t),
                static_cast<float>(w*t-x*u-y*v-z*s)};
    }
    // PC 0x464C10 / 0x464B90: the original does not normalize or clamp here.
    inline Quaternion Log(const Quaternion& q)
    {
        const float angle = std::acos(q[3]), sine = std::sin(angle);
        const float factor = std::fabs(sine) < 0.001F ? 1.0F : angle / sine;
        return {q[0] * factor, q[1] * factor, q[2] * factor, 0};
    }
    inline Quaternion Exp(const Quaternion& q)
    {
        const float angle = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        const float sine = std::sin(angle);
        const float factor = std::fabs(sine) < 0.001F ? 1.0F : sine / angle;
        return {q[0] * factor, q[1] * factor, q[2] * factor, std::cos(angle)};
    }
    // PC 0x464DE0; prepare 0x4933C0 repeats endpoint values as neighbours.
    inline Quaternion Control(const Quaternion& previous, const Quaternion& current,
                              const Quaternion& next)
    {
        const Quaternion inverse{-current[0], -current[1], -current[2], current[3]};
        const auto left = Log(Multiply(inverse, previous)), right = Log(Multiply(inverse, next));
        Quaternion sum{};
        for (std::size_t i = 0; i < 4; ++i)
            sum[i] = -0.25F * (left[i] + right[i]);
        return Multiply(current, Exp(sum));
    }
    inline Quaternion Interpolate(const Quaternion& first, Quaternion second, const float factor)
    {
        // PC4648C0 accumulates on x87, then spills ONCE at464901 before
        // acos. Rounding each product (or never spilling the sum) changes
        // the sine<0.001 branch on Icy L_Index_02 at t=1.
        float dot = static_cast<float>(double(first[2]) * second[2] + double(first[1]) * second[1] +
                                      double(first[0]) * second[0] + double(first[3]) * second[3]);
        if (dot < 0.0F)
        {
            for (auto& value : second)
                value = -value;
            dot = -dot;
        }
        if (dot > 1.0F)
            dot = 1.0F;
        const float theta = static_cast<float>(std::acos(double(dot))); //46497D
        const double sine = std::sin(double(theta));
        // The native helper copies first here; it does not use nlerp.
        if (sine < 0.001F)
            return first;
        const float inverse = static_cast<float>(1.0 / sine); //4649BA
        const float angleA = static_cast<float>((1.0-double(factor))*theta);
        const float angleB = static_cast<float>(double(factor)*theta);
        const float a = static_cast<float>(std::sin(double(angleA))*inverse); //4649DA
        const double b = std::sin(double(angleB))*inverse;
        // XY spill both products, Z retains the second, W retains the first.
        return {static_cast<float>(double(static_cast<float>(double(first[0])*a))+static_cast<float>(second[0]*b)),
                static_cast<float>(double(static_cast<float>(double(first[1])*a))+static_cast<float>(second[1]*b)),
                static_cast<float>(double(static_cast<float>(double(first[2])*a))+second[2]*b),
                static_cast<float>(double(first[3])*a+static_cast<float>(second[3]*b))};
    }
    inline Matrix3 ToMatrix(const Quaternion& q)
    {
        // PC4647F0 stores selected products as float but retains yy/yz/zz
        // on the x87 stack. CP22 UV vectors expose premature host rounding.
        const double x=q[0],y=q[1],z=q[2],w=q[3];
        const float z2=static_cast<float>(2*z),xx=static_cast<float>(2*x*x);
        const double yy=2*y*y,yz=static_cast<double>(z2)*y,zz=static_cast<double>(z2)*z;
        const float xy=static_cast<float>(2*y*x),xz=static_cast<float>(static_cast<double>(z2)*x);
        const float wx=static_cast<float>(2*x*w),wy=static_cast<float>(2*y*w),wz=static_cast<float>(static_cast<double>(z2)*w);
        return {static_cast<float>(1-(zz+yy)),static_cast<float>(static_cast<double>(xy)+wz),static_cast<float>(static_cast<double>(xz)-wy),
            static_cast<float>(static_cast<double>(xy)-wz),static_cast<float>(1-(zz+xx)),static_cast<float>(yz+wx),
            static_cast<float>(static_cast<double>(xz)+wy),static_cast<float>(yz-wx),static_cast<float>(1-(yy+xx))};
    }
    inline Quaternion FromMatrix(const Matrix3& m)
    {
        Quaternion q{};
        // PC464CB0 keeps trace, square root and reciprocal on the x87 stack.
        // CP89 real SMO light writer exposed an extra ULP from float temporaries.
        // Double retains the tested finite results; not a universal x87 claim.
        const double trace = double(m[4]) + double(m[0]) + double(m[8]);
        if (trace > 0)
        {
            const double root = std::sqrt(trace + 1);
            q[3] = static_cast<float>(0.5 * root);
            const double factor = 0.5 / root;
            q[0] = static_cast<float>((double(m[5]) - m[7]) * factor);
            q[1] = static_cast<float>((double(m[6]) - m[2]) * factor);
            q[2] = static_cast<float>((double(m[1]) - m[3]) * factor);
        }
        else
        {
            std::size_t i = m[4] > m[0] ? 1 : 0;
            if (m[8] > m[4 * i])
                i = 2;
            const auto j = (i + 1) % 3, k = (j + 1) % 3;
            const double root = std::sqrt(double(m[4 * i]) - m[4 * j] - m[4 * k] + 1);
            q[i] = static_cast<float>(0.5 * root);
            const double factor = 0.5 / root;
            q[3] = static_cast<float>((double(m[3 * j + k]) - m[3 * k + j]) * factor);
            q[j] = static_cast<float>((double(m[3 * i + j]) + m[3 * j + i]) * factor);
            q[k] = static_cast<float>((double(m[3 * i + k]) + m[3 * k + i]) * factor);
        }
        return q;
    }
} // namespace sparkplug::evidence::pc::animation_math
