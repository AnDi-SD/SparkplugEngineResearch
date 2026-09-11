#pragma once
// PC4620C0. Analytical API; explicit original float spills, finite inputs.
// Host double sin/cos substitutes x87 transcendental instructions. Validation
// covers captured float results, not every possible x87 input bit pattern.
#include "spNodeTransformMath.h"

namespace sparkplug::evidence::pc::node_math
{
    inline Matrix3 AxisAngleForAnalysis(float angle,const Vector3& axis)
    {
        const double cosine=std::cos(double(angle)),sine=std::sin(double(angle));
        const double oneMinus=1.0-cosine;
        const float t=float(oneMinus),xy=float(oneMinus*axis[0]*axis[1]);
        const double xz=oneMinus*axis[0]*axis[2];
        const float yz=float(double(t)*axis[1]*axis[2]),sx=float(sine*axis[0]);
        const double sy=sine*axis[1],sz=sine*axis[2];
        return {float(double(axis[0])*axis[0]*t+cosine),float(double(xy)+sz),float(xz-sy),
                float(double(xy)-sz),float(double(axis[1])*axis[1]*t+cosine),float(double(sx)+yz),
                float(xz+sy),float(double(yz)-sx),float(double(axis[2])*axis[2]*t+cosine)};
    }
}
