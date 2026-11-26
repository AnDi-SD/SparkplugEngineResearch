#include "spFunctionEvalLinearRampPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (magnitude >= 0x00800000u && magnitude <= 0x46800000u);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float word;
            std::memcpy(&word, &bits, sizeof(word));
            return math::From(word);
        }

        FunctionEvalLinearRampPc80ForAnalysis Encode(math::Number value) noexcept
        {
            constexpr int Binary80Bias = 16383;
            auto exponent = static_cast<std::uint16_t>(value.mantissa ? value.exponent + 63 + Binary80Bias : 0);
            if (value.negative)
                exponent |= 0x8000u;

            FunctionEvalLinearRampPc80ForAnalysis bytes{};
            for (std::size_t i = 0; i < 8; ++i)
                bytes[i] = static_cast<std::uint8_t>(value.mantissa >> (8 * i));
            bytes[8] = static_cast<std::uint8_t>(exponent);
            bytes[9] = static_cast<std::uint8_t>(exponent >> 8);
            return bytes;
        }
    }

    bool TryEvaluateFunctionEvalLinearRampPcForAnalysis(const FunctionEvalLinearRampPcInputForAnalysis& input,
        FunctionEvalLinearRampPc80ForAnalysis& output) noexcept
    {
        if (!Admitted(input.storedTimeBits) || !Admitted(input.pitchBits) ||
            !Admitted(input.yOffsetBits) || !Admitted(input.clampLimitBits))
            return false;

        const auto pitch = Widen(input.pitchBits);
        const auto product = math::Multiply(pitch, Widen(input.storedTimeBits));
        auto result = math::Add(product, Widen(input.yOffsetBits));

        if (input.clampEnabled != 0 && pitch.mantissa != 0)
        {
            const auto limit = Widen(input.clampLimitBits);
            const auto ordering = math::nearest::Compare(result, limit);
            if ((pitch.negative && ordering <= 0) || (!pitch.negative && ordering >= 0))
                result = limit;
        }

        output = Encode(result);
        return true;
    }
}
