#include "spPS2MeshNormalPacking.h"

namespace sparkplug::evidence::ps2
{
    namespace
    {
        struct Dyadic final { bool negative; std::uint64_t magnitude; int exponent; };
        bool Decode(std::uint32_t word, Dyadic& out) noexcept
        {
            const auto exponent = (word >> 23) & 255U;
            const auto fraction = word & 0x7FFFFFU;
            if (exponent == 255 || (exponent == 0 && fraction != 0)) return false;
            out = {(word >> 31) != 0, exponent ? 0x800000U | fraction : 0U,
                exponent ? static_cast<int>(exponent) - 150 : 0};
            return true;
        }
        bool Encode(Dyadic source, std::uint32_t& result) noexcept
        {
            const auto sign = source.negative ? 0x80000000U : 0U;
            if (!source.magnitude) { result = sign; return true; }
            unsigned top = 0;
            for (auto value = source.magnitude; value > 1; value >>= 1) ++top;
            const int exponent = source.exponent + static_cast<int>(top);
            if (exponent < -126 || exponent > 127) return false;
            std::uint64_t significand = source.magnitude;
            if (top > 23)
            {
                const auto shift = top - 23;
                if (significand & ((std::uint64_t{1} << shift) - 1)) return false;
                significand >>= shift;
            }
            else significand <<= 23 - top;
            result = sign | (static_cast<std::uint32_t>(exponent + 127) << 23)
                | (static_cast<std::uint32_t>(significand) & 0x7FFFFFU);
            return true;
        }
    }

    bool ExactMeshNormalArithmeticForAnalysis::TryAddSingle(
        std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept
    {
        Dyadic a{}, b{};
        if (!Decode(first, a) || !Decode(second, b)) return false;
        if (!a.magnitude) { result = second; return true; }
        if (!b.magnitude) { result = first; return true; }
        const auto exponent = a.exponent < b.exponent ? a.exponent : b.exponent;
        const auto shiftA = static_cast<unsigned>(a.exponent - exponent);
        const auto shiftB = static_cast<unsigned>(b.exponent - exponent);
        // Both significands have24bits. A wider span cannot produce an exact
        // binary32 sum here: operands are too far apart for cancellation.
        if (shiftA > 39 || shiftB > 39) return false;
        const auto wideA = a.magnitude << shiftA;
        const auto wideB = b.magnitude << shiftB;
        Dyadic sum{a.negative, 0, exponent};
        if (a.negative == b.negative) sum.magnitude = wideA + wideB;
        else if (wideA >= wideB) sum.magnitude = wideA - wideB;
        else { sum.magnitude = wideB - wideA; sum.negative = b.negative; }
        if (!sum.magnitude) sum.negative = false; // Packing/CVT ignores zero sign.
        return Encode(sum, result);
    }

    bool ExactMeshNormalArithmeticForAnalysis::TrySubtractSingle(
        std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept
    { return TryAddSingle(first, second ^ 0x80000000U, result); }

    bool ExactMeshNormalArithmeticForAnalysis::TryMultiplySingle(
        std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept
    {
        Dyadic a{}, b{};
        if (!Decode(first, a) || !Decode(second, b)) return false;
        return Encode({a.negative != b.negative, a.magnitude * b.magnitude, a.exponent + b.exponent}, result);
    }

    bool ExactMeshNormalArithmeticForAnalysis::TryLessEqualSingle(
        std::uint32_t first, std::uint32_t second, bool& result) const noexcept
    {
        Dyadic a{}, b{};
        if (!Decode(first, a) || !Decode(second, b)) return false;
        const auto key = [](std::uint32_t word) { return word & 0x80000000U ? ~word : word | 0x80000000U; };
        result = (!a.magnitude && !b.magnitude) || key(first) <= key(second);
        return true;
    }

    bool ExactMeshNormalArithmeticForAnalysis::TryConvertToWord(
        std::uint32_t source, std::uint32_t& result) const noexcept
    {
        Dyadic value{};
        if (!Decode(source, value)) return false;
        const auto exponent = static_cast<int>((source >> 23) & 255U) - 127;
        if (exponent >= 31) result = value.negative ? 0x80000000U : 0x7FFFFFFFU;
        else if (exponent < 0) result = 0;
        else
        {
            const auto integer = static_cast<std::uint32_t>(exponent >= 23
                ? value.magnitude << (exponent - 23) : value.magnitude >> (23 - exponent));
            result = value.negative ? 0U - integer : integer;
        }
        return true;
    }
}
