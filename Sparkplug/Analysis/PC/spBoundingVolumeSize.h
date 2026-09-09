#pragma once
// Analytical shared extraction of the identical scalar size bodies at
// PC4394EA..439535 (Box) and PC439C47..439C92 (OBB); path/name inferred.
#include <array>
#include <cmath>

namespace sparkplug::evidence::pc::bounding_volume
{
    struct SizeState final
    {
        std::array<float,3> halfExtents;
        float boundingSphereRadius;
    };

    inline SizeState DecodeSize(const std::array<float,3>& fullSize) noexcept
    {
        // Both originals retain the halved inputs, squares and z*z+y*y+x*x
        // on x87 until the final radius float store. Float-only arithmetic
        // gives 3F0DF578 instead of the observed 3F0DF579 for (.1,.1,1.1).
        // Half extents are an inspector projection, not a native store here.
        // Wider intermediates also avoid artificial float square overflow/
        // underflow; this is not a universal x87 bit-identity claim.
        const double x=double(fullSize[0])*0.5,y=double(fullSize[1])*0.5,z=double(fullSize[2])*0.5;
        return {{{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)}},
            static_cast<float>(std::sqrt((z*z+y*y)+x*x))};
    }
}
