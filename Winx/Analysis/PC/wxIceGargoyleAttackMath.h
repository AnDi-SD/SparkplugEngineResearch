#pragma once

// Own PC four-stage arithmetic, expressed through the existing shared
// nearest64 operations. Normalization retains the existing conditional
// finite-input adapter; this does not establish arbitrary x87/CRT identity.
#include "Analysis/PC/spOcclusionSceneMath.h"
#include "Analysis/PC/wxButterflyNumeric.h"
#include <array>
#include <bit>
#include <cmath>

namespace winx::analysis::pc::ice_gargoyle_attack_math
{
    namespace arithmetic=sparkplug::evidence::pc::transform_const_math;
    using arithmetic::Number;
    using arithmetic::AddNearest;
    using arithmetic::MultiplyFloatNearest;
    using arithmetic::StoreNearest;
    using sparkplug::evidence::pc::float80_rtz::FromFloat;
    using sparkplug::evidence::pc::occlusion_scene_math::SquareNearest;
    using Vector=std::array<float,3>;
    inline Number Product(Number first,float second) noexcept
    {
        auto result=MultiplyFloatNearest(first,second);
        // The shared finite helper collapses a zero product to positive0.
        // This consumer stores signed-zero speed, so retain operand sign.
        if (!result.mantissa) result.negative=first.negative!=std::signbit(second);
        return result;
    }
    inline Number Negate(Number value) noexcept { value.negative=!value.negative;return value; }
    inline Number Difference(float a,float b) noexcept { return AddNearest(FromFloat(a),Negate(FromFloat(b))); }
    inline bool Bounded(float value,float limit=4096.0f) noexcept
    { return std::isfinite(value) && std::abs(value)<=limit; }
    inline bool Bounded(const Vector& value,float limit=4096.0f) noexcept
    { for (const float v:value) if (!Bounded(v,limit)) return false;return true; }
    inline Vector Floats(const std::array<std::uint32_t,3>& words) noexcept
    { return {std::bit_cast<float>(words[0]),std::bit_cast<float>(words[1]),std::bit_cast<float>(words[2])}; }
    inline std::array<std::uint32_t,3> Words(const Vector& value) noexcept
    { return {std::bit_cast<std::uint32_t>(value[0]),std::bit_cast<std::uint32_t>(value[1]),std::bit_cast<std::uint32_t>(value[2])}; }
    inline bool Direction(const Vector& origin,const Vector& target,Vector& result,bool planar) noexcept
    {
        if (!Bounded(origin) || !Bounded(target)) return false;
        for (unsigned axis=0;axis!=3;++axis)
            result[axis]=planar && axis==1?0.0f:StoreNearest(Difference(target[axis],origin[axis]));
        winx::evidence::pc::ButterflyNormalizeForAnalysis(result);
        return true;
    }
    inline bool SquaredLess(const Vector& origin,const Vector& target,float limit,bool& result) noexcept
    {
        if (!Bounded(origin) || !Bounded(target)) return false;
        const auto x=Difference(origin[0],target[0]),y=Difference(origin[1],target[1]),z=Difference(origin[2],target[2]);
        result=arithmetic::Compare(AddNearest(AddNearest(SquareNearest(z),SquareNearest(y)),SquareNearest(x)),FromFloat(limit))<0;
        return true;
    }
    inline Number FromU32(std::uint32_t value) noexcept
    { return value?arithmetic::NormalizeNearest(arithmetic::Wide::Shifted(value,0),0,false):Number{}; }
    inline float RandomCoordinate(std::uint32_t value,float factor,float bias) noexcept
    { return StoreNearest(AddNearest(Product(FromU32(value),factor),FromFloat(bias))); }
    inline float Accelerate(float speed,float dt,float acceleration) noexcept
    { return StoreNearest(AddNearest(FromFloat(speed),Product(FromFloat(dt),acceleration))); }
    inline Vector MoveWaypoint(const Vector& origin,const Vector& direction,float speed,float dt) noexcept
    {
        // Distinct original spills: X velocity stays extended; Y/Z velocity
        // spill. X/Y delta spill, while Z delta stays extended through origin.
        const auto vx=Product(FromFloat(speed),direction[0]);
        const float vy=StoreNearest(Product(FromFloat(speed),direction[1]));
        const float vz=StoreNearest(Product(FromFloat(speed),direction[2]));
        const float dx=StoreNearest(Product(vx,dt));
        const float dy=StoreNearest(Product(FromFloat(vy),dt));
        const auto dz=Product(FromFloat(dt),vz);
        return {StoreNearest(AddNearest(FromFloat(dx),FromFloat(origin[0]))),
            StoreNearest(AddNearest(FromFloat(dy),FromFloat(origin[1]))),
            StoreNearest(AddNearest(dz,FromFloat(origin[2])))};
    }
    inline float FallY(float y,float storedSpeed,float dt) noexcept
    { return StoreNearest(AddNearest(Product(FromFloat(dt),storedSpeed),FromFloat(y))); }
    struct ThresholdUpdate final { float stored;bool clamp; };
    inline ThresholdUpdate NotifyThreshold(float old) noexcept
    {
        const auto sum=AddNearest(FromFloat(old),FromFloat(0.2f));
        const float stored=StoreNearest(sum);
        // Native FST writes the rounded field but keeps unspilled ST0 for
        // the following strict comparison against float0.8.
        return {stored,arithmetic::Compare(sum,FromFloat(0.8f))>0};
    }
}
