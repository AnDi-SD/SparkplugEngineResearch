#include "spAnimationQuaternionSquadFactorPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    bool TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis(
        std::uint32_t intervalFactorBits, std::uint32_t& blendFactorBits) noexcept
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        const bool zero = (intervalFactorBits & 0x7FFFFFFFu) == 0;
        const bool positiveAdmitted = 0x1F800000u <= intervalFactorBits && intervalFactorBits <= 0x3F800000u;
        if (!zero && !positiveAdmitted)
            return false;

        float storedFactor;
        std::memcpy(&storedFactor, &intervalFactorBits, sizeof(storedFactor));
        const auto factor = math::From(storedFactor);
        const auto complement = math::Subtract(math::From(1.0f), factor);
        const auto product = math::Multiply(complement, factor);
        const auto blended = math::Store(math::Add(product, product));
        std::memcpy(&blendFactorBits, &blended, sizeof(blendFactorBits));
        return true;
    }
}
