#pragma once

// PC420710 and420240; finite inputs/default nearest64 x87 rounding.
// Reuses shared integer arithmetic; no host long-double assumption or NaN/Inf model.
#include "spTransformConstMath.h"
#include <array>
#include <cmath>

namespace sparkplug::evidence::pc::occlusion_camera_math
{
    namespace arithmetic=transform_const_math;
    using arithmetic::Number;
    using arithmetic::AddNearest;
    using arithmetic::StoreNearest;
    using arithmetic::Compare;
    using float80_rtz::FromFloat;
    using Vector3=std::array<float,3>;
    using Matrix3=std::array<float,9>;
    inline Number Negate(Number value) noexcept{value.negative=!value.negative;return value;}
    inline Number Subtract(Number a,Number b) noexcept{return AddNearest(a,Negate(b));}
    inline Number Multiply(Number a,float b) noexcept
    {
        // The shared nonzero significand arithmetic is reused. x87 preserves
        // a zero product's XOR sign, required for signed-scale inverse stores.
        if(!a.mantissa||b==0)return {0,0,a.negative!=std::signbit(b)};
        return arithmetic::MultiplyFloatNearest(a,b);
    }
    template<std::size_t N> inline bool Finite(const std::array<float,N>& value) noexcept
    {for(float f:value)if(!std::isfinite(f))return false;return true;}
    template<std::size_t N> inline bool Near(const std::array<float,N>& a,
                                          const std::array<float,N>& b) noexcept
    {
        //420240: <=epsilon on every component; an unordered difference fails.
        for(std::size_t i=0;i<N;++i)
        {
            if(!std::isfinite(a[i])||!std::isfinite(b[i]))return false;
            auto difference=Subtract(FromFloat(a[i]),FromFloat(b[i]));difference.negative=false;
            if(Compare(difference,FromFloat(.001f))>0)return false;
        }
        return true;
    }
    inline bool WorldToLocal420710(const Vector3& input,const Vector3& position,
        const Matrix3& orientation,const Vector3& scale,Vector3& output) noexcept
    {
        if(!Finite(input)||!Finite(position)||!Finite(orientation)||!Finite(scale))return false;
        for(float value:scale)if(value==0)return false; // host domain, native Inf/NaN is not modeled
        std::array<Number,3> inverse,delta;
        for(unsigned i=0;i<3;++i)
        {
            inverse[i]=arithmetic::DividePositiveFloatNearest(FromFloat(1.f),std::abs(scale[i]));
            inverse[i].negative=std::signbit(scale[i]);
            if(i)
            {
                const float spilled=StoreNearest(inverse[i]);
                if(!std::isfinite(spilled))return false;
                inverse[i]=FromFloat(spilled);
            }
            delta[i]=Subtract(FromFloat(input[i]),FromFloat(position[i]));
        }
        Vector3 result;
        for(unsigned row=0;row<3;++row)
        {
            // All three rotations spill to float before scaling. ReciprocalX
            // stays extended; reciprocalY/Z were stored to float above.
            const auto sum=AddNearest(AddNearest(Multiply(delta[2],orientation[row*3+2]),
                Multiply(delta[1],orientation[row*3+1])),Multiply(delta[0],orientation[row*3]));
            const float rotated=StoreNearest(sum);
            if(!std::isfinite(rotated))return false;
            result[row]=StoreNearest(Multiply(inverse[row],rotated));
        }
        if(!Finite(result))return false;
        output=result;return true;
    }
    inline Number FaceDistance(const std::array<float,4>& plane,const Vector3& point) noexcept
    {
        return Subtract(AddNearest(AddNearest(Multiply(FromFloat(point[2]),plane[2]),
            Multiply(FromFloat(point[1]),plane[1])),Multiply(FromFloat(point[0]),plane[0])),FromFloat(plane[3]));
    }
}
