#include "spAnimationScalarCubicSamplePc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr std::uint32_t MagnitudeMask = 0x7FFFFFFFu;
        constexpr std::uint32_t MinimumWordMagnitude = 0x1F800000u;
        constexpr std::uint32_t MinimumFactorMagnitude = 0x2F800000u;
        constexpr std::uint32_t MaximumWordMagnitude = 0x45800000u;
        constexpr std::uint32_t OneBits = 0x3F800000u;

        constexpr bool AdmittedWord(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & MagnitudeMask;
            return magnitude == 0 || (MinimumWordMagnitude <= magnitude && magnitude <= MaximumWordMagnitude);
        }

        constexpr bool AdmittedFactor(std::uint32_t bits) noexcept
        {
            return (bits & MagnitudeMask) == 0 || (MinimumFactorMagnitude <= bits && bits <= OneBits);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }
    }

    bool TrySampleAnimationScalarCubicPcForAnalysis(
        const AnimationScalarCubicKeyPcForAnalysis& record,
        std::uint32_t factorBits,
        std::uint32_t& outputBits) noexcept
    {
        if (!AdmittedFactor(factorBits) || !AdmittedWord(record.valueBits) ||
            !AdmittedWord(record.outgoingBits) || !AdmittedWord(record.quadraticBits) || !AdmittedWord(record.cubicBits))
            return false;

        const auto factor = Widen(factorBits);
        const auto quadratic = math::Add(math::Multiply(factor, Widen(record.cubicBits)), Widen(record.quadraticBits));
        const auto outgoing = math::Add(math::Multiply(quadratic, factor), Widen(record.outgoingBits));
        const auto value = math::Add(math::Multiply(outgoing, factor), Widen(record.valueBits));
        const auto rounded = math::Store(value);
        std::memcpy(&outputBits, &rounded, sizeof(outputBits));
        return true;
    }
}
