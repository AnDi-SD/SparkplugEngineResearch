#include "spAnimationScalarCubicPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;

        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
        static_assert(sizeof(AnimationScalarCubicKeyPcForAnalysis) == 20);
        static_assert(offsetof(AnimationScalarCubicKeyPcForAnalysis, quadraticBits) == 12);
        static_assert(offsetof(AnimationScalarCubicKeyPcForAnalysis, cubicBits) == 16);

        constexpr bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (magnitude >= 0x1F800000u && magnitude <= 0x45800000u);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto rounded = math::Store(value);
            std::uint32_t bits;
            std::memcpy(&bits, &rounded, sizeof(bits));
            return bits;
        }
    }

    bool TryPrepareAnimationScalarCubicPcForAnalysis(
        AnimationScalarCubicKeyPcForAnalysis* keys, std::size_t keyCount) noexcept
    {
        constexpr auto MaximumCount = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
        if (keys == nullptr || keyCount == 0 || keyCount > MaximumCount)
            return false;

        const auto segmentCount = keyCount - 1;
        for (std::size_t i = 0; i < segmentCount; ++i)
        {
            if (!Admitted(keys[i].valueBits) || !Admitted(keys[i + 1].valueBits) ||
                !Admitted(keys[i].outgoingBits) || !Admitted(keys[i + 1].incomingBits))
                return false;
        }

        const auto three = math::From(3.0f);
        for (std::size_t i = 0; i < segmentCount; ++i)
        {
            auto& current = keys[i];
            const auto& next = keys[i + 1];
            const auto delta = math::Subtract(Widen(next.valueBits), Widen(current.valueBits));
            const auto outgoing = Widen(current.outgoingBits);
            const auto incoming = Widen(next.incomingBits);
            const auto tangent = math::Add(math::Add(outgoing, outgoing), incoming);
            current.quadraticBits = StoreWord(math::Subtract(math::Multiply(delta, three), tangent));
            current.cubicBits = StoreWord(
                math::Subtract(math::Add(incoming, outgoing), math::Add(delta, delta)));
        }
        return true;
    }
}
