#pragma once

// PC46CA44..46CD5B. Native near-plane-band work uses nearest64 x87
// intermediates and distinct float spills; reuse the existing integer model.
// Finite inputs/default rounding only, not an x87 environment emulator.
#include "spTransformConstMath.h"

namespace sparkplug::evidence::pc::visibility_portal_math
{
    using Vector3=std::array<float,3>;
    namespace arithmetic=transform_const_math;
    using arithmetic::Number;
    using arithmetic::AddNearest;
    using arithmetic::MultiplyFloatNearest;
    using arithmetic::StoreNearest;
    using arithmetic::Compare;
    using float80_rtz::FromFloat;
    inline Number Negate(Number value) noexcept {value.negative=!value.negative;return value;}
    inline Number Subtract(Number a,Number b) noexcept {return AddNearest(a,Negate(b));}
    inline float Difference(float a,float b) noexcept
    {return StoreNearest(Subtract(FromFloat(a),FromFloat(b)));}
    inline Vector3 AddCameraOffset(const Vector3& camera,const Vector3& offset) noexcept
    {
        Vector3 result;
        for(unsigned i=0;i<3;++i)result[i]=StoreNearest(AddNearest(FromFloat(camera[i]),FromFloat(offset[i])));
        return result;
    }
    inline Number PortalDistance(const std::array<float,4>& plane,const Vector3& point) noexcept
    {
        return Subtract(AddNearest(AddNearest(MultiplyFloatNearest(FromFloat(point[1]),plane[1]),
            MultiplyFloatNearest(FromFloat(point[2]),plane[2])),
            MultiplyFloatNearest(FromFloat(point[0]),plane[0])),FromFloat(plane[3]));
    }
    struct RelativeVertex
    {
        Number x;
        float y=0,z=0;
    };
    inline RelativeVertex Relative(const Vector3& point,const Vector3& camera,bool spillX) noexcept
    {
        auto x=Subtract(FromFloat(point[0]),FromFloat(camera[0]));
        if(spillX)x=FromFloat(StoreNearest(x));
        return {x,Difference(point[1],camera[1]),Difference(point[2],camera[2])};
    }
    inline bool EdgeFacesCamera(const RelativeVertex& previous,const RelativeVertex& next,
        const std::array<float,4>& plane,bool closing) noexcept
    {
        // Ordinary edges spill cross.z, retain cross.x/y. Closing edge sums
        // y+z before x; ordinary edges sum x+y before z. Initial relative.x
        // stays extended until the final closing edge.
        const auto z=FromFloat(StoreNearest(Subtract(MultiplyFloatNearest(previous.x,next.y),
            MultiplyFloatNearest(next.x,previous.y))));
        const auto y=Subtract(MultiplyFloatNearest(next.x,previous.z),MultiplyFloatNearest(previous.x,next.z));
        const auto x=Subtract(MultiplyFloatNearest(FromFloat(next.z),previous.y),
            MultiplyFloatNearest(FromFloat(next.y),previous.z));
        const auto nx=MultiplyFloatNearest(x,plane[0]);
        const auto ny=MultiplyFloatNearest(y,plane[1]);
        const auto nz=MultiplyFloatNearest(z,plane[2]);
        const auto dot=closing?AddNearest(AddNearest(ny,nz),nx):AddNearest(AddNearest(nx,ny),nz);
        return Compare(dot,{})>=0;
    }
    inline Vector3 ShiftedIncoming(const Vector3& camera,const Vector3& incoming,
        const std::array<float,4>& plane) noexcept
    {
        Vector3 result;
        for(unsigned i=0;i<3;++i)
        {
            auto shift=MultiplyFloatNearest(FromFloat(plane[i]),0.002f);
            if(i==2)shift=FromFloat(StoreNearest(shift));
            const float displaced=StoreNearest(Subtract(FromFloat(camera[i]),shift));
            result[i]=StoreNearest(AddNearest(FromFloat(displaced),FromFloat(incoming[i])));
        }
        return result;
    }
}
