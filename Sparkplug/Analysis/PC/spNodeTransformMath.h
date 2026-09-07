#pragma once

// Analytical names for PC 0x420350, 0x420C00, 0x4207E0 and 0x461D70.
// Finite-input reconstruction, not a bit-identical x87 implementation.
#include <array>
#include <cmath>
#include <cstddef>

namespace sparkplug::evidence::pc::node_math
{
    using Vector3 = std::array<float, 3>;
    using Matrix3 = std::array<float, 9>;
    using Matrix4 = std::array<float, 16>;
    inline constexpr Matrix3 Identity{1, 0, 0, 0, 1, 0, 0, 0, 1};

    // PC426B00: shared four-by-four multiplication used by UV/TransFunction.
    // Original compiler uses a distinct addition order for individual cells;
    // retain wider products and round only each destination store.
    inline Matrix4 Multiply4ForAnalysis(const Matrix4& a,const Matrix4& b)
    {
        constexpr unsigned order[16][4]={{2,1,0,3},{1,2,0,3},{1,2,0,3},{1,2,0,3},
            {1,3,2,0},{1,0,3,2},{1,0,3,2},{1,0,3,2},
            {2,1,3,0},{3,2,0,1},{3,2,0,1},{3,2,0,1},
            {0,3,1,2},{2,1,3,0},{2,1,3,0},{1,3,2,0}};
        Matrix4 result{};
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        {
            const auto& o=order[r*4+c];
            double value=static_cast<double>(a[r*4+o[0]])*b[o[0]*4+c];
            for(unsigned k=1;k<4;++k)value+=static_cast<double>(a[r*4+o[k]])*b[o[k]*4+c];
            result[r*4+c]=static_cast<float>(value);
        }
        return result;
    }

    inline Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
    inline float Normalize(Vector3& value)
    {
        const float length =
            std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
        if (length > 0.001F)
            for (auto& v : value)
                v /= length;
        else
            value = {0, 0, 0};
        return length;
    }
    inline Vector3 Transform(const Vector3& v, const Matrix3& m)
    {
        Vector3 result{};
        // PC420350 accumulates k=2,1,0 on x87 and rounds only the final
        // component store. CP107 droid_trail exposes cancellation differences.
        for (std::size_t c = 0; c < 3; ++c)
            result[c] = static_cast<float>((double(v[2]) * m[6 + c]
                + double(v[1]) * m[3 + c]) + double(v[0]) * m[c]);
        return result;
    }
    inline Matrix3 Multiply(const Matrix3& a, const Matrix3& b)
    {
        Matrix3 result{};
        // PC420C00 has a different order in individual destination cells.
        constexpr unsigned order[9][3]={{1,0,2},{2,0,1},{2,0,1},
            {2,1,0},{2,1,0},{2,1,0},{2,0,1},{0,2,1},{0,2,1}};
        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t c = 0; c < 3; ++c)
            {
                const auto& o=order[r*3+c];
                double value=double(a[r*3+o[0]])*b[o[0]*3+c];
                for(unsigned k=1;k<3;++k)value+=double(a[r*3+o[k]])*b[o[k]*3+c];
                result[r*3+c]=static_cast<float>(value);
            }
        return result;
    }
    inline Matrix4 Affine(const Vector3& position, const Matrix3& orientation, const Vector3& scale)
    {
        Matrix4 result{};
        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t c = 0; c < 3; ++c)
                result[r * 4 + c] = scale[r] * orientation[r * 3 + c];
        for (std::size_t c = 0; c < 3; ++c)
            result[12 + c] = position[c];
        result[15] = 1;
        return result;
    }
    inline bool Billboard(const Matrix3* camera, unsigned axis, Matrix3& result)
    {
        if (!camera)
        {
            result = Identity;
            return true;
        }
        // Host-only finite/degeneracy guard: native 0x420470 has an
        // uninitialized-stack branch for coincident basis vectors. Do not
        // represent that undefined input behavior as a reconstructed rule.
        for (float value : *camera)
            if (!std::isfinite(value))
                return false;
        Vector3 forward{-(*camera)[6], -(*camera)[7], -(*camera)[8]}, up{0, 1, 0};
        if (Normalize(forward) <= 0.001F)
            return false;
        if (std::fabs(std::fabs(forward[1]) - 1) < 0.001F)
        {
            Vector3 cameraUp{(*camera)[3], (*camera)[4], (*camera)[5]};
            if (Normalize(cameraUp) <= 0.001F)
                return false;
            if (axis == 1)
                forward = {-cameraUp[0], -cameraUp[1], -cameraUp[2]};
            else
                up = cameraUp;
        }
        auto right = Cross(up, forward);
        if (Normalize(right) <= 0.001F)
            return false;
        if (axis == 1)
        {
            forward = Cross(right, up);
            Normalize(forward);
        }
        else
        {
            up = Cross(forward, right);
            Normalize(up);
        }
        right = Cross(up, forward); // 0x420470 recomputes this row without normalization.
        result = {right[0], right[1],   right[2],   up[0],     up[1],
                  up[2],    forward[0], forward[1], forward[2]};
        return true;
    }
} // namespace sparkplug::evidence::pc::node_math
