#pragma once
// Shared analytical qualification of exact integral Mouse COP1 operations.
// Initialized polling keeps X/Y within320/224 and signed-byte additions are
// exactly representable singles. Normal finite CVT.W.S truncates and saturates
// under EE fixed rounding-towards-zero. Not a general R5900 FPU/status model.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>

namespace sparkplug::evidence::ps2::mouse_exact_integer_math
{
    inline float Float(std::uint32_t bits) noexcept
    {float value;std::memcpy(&value,&bits,4);return value;}
    inline std::uint32_t Bits(float value) noexcept
    {std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    inline bool Integral(std::uint32_t bits) noexcept
    {const float value=Float(bits);return std::isfinite(value)&&std::trunc(value)==value;}
    inline bool NormalOrZero(std::uint32_t bits) noexcept
    {return std::isfinite(Float(bits))&&((bits&0x7F800000U)!=0||(bits&0x7FFFFFU)==0);}
    inline std::optional<std::uint32_t> AddSignedByte(std::uint32_t bits,std::int8_t offset) noexcept
    {
        if(!Integral(bits))return std::nullopt;
        const double result=double(Float(bits))+offset;
        if(std::abs(result)>16777216.0)return std::nullopt;
        // The converted integer offset has positive zero, so zero/cancellation
        // yields positive zero under the EE fixed towards-zero mode.
        if(result==0.0)return 0U;
        return Bits(static_cast<float>(result));
    }
    inline std::optional<std::uint32_t> SubtractSingle(std::uint32_t first,std::uint32_t second) noexcept
    {
        if(!Integral(first)||!Integral(second))return std::nullopt;
        const double result=double(Float(first))-Float(second);
        if(result==0.0)return first==0x80000000U&&second==0U?0x80000000U:0U;
        const float stored=static_cast<float>(result);
        if(!std::isfinite(stored)||double(stored)!=result)return std::nullopt;
        return Bits(stored);
    }
    inline std::optional<std::int32_t> ConvertSingleToWord(std::uint32_t bits) noexcept
    {
        // EE denormals/encoded255 values have distinct unsupported semantics.
        // Normal finite overflow has the documented signed-word saturation.
        if(!NormalOrZero(bits))return std::nullopt;
        const double value=Float(bits);
        if(value<=-2147483648.0)return (-2147483647-1);
        if(value>=2147483648.0)return 2147483647;
        return static_cast<std::int32_t>(value);
    }
    inline std::optional<bool> CompareLess(std::uint32_t first,std::uint32_t second) noexcept
    {if(!NormalOrZero(first)||!NormalOrZero(second))return std::nullopt;return Float(first)<Float(second);}
    inline std::optional<bool> CompareLessEqual(std::uint32_t first,std::uint32_t second) noexcept
    {if(!NormalOrZero(first)||!NormalOrZero(second))return std::nullopt;return Float(first)<=Float(second);}
}
