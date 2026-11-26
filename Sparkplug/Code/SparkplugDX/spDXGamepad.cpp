#include "spDXGamepad.h"

#include "../../Analysis/PC/spDXGamepadAbi.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace Abi = sparkplug::evidence::pc;

        struct Axis
        {
            std::size_t offset;
            bool negate;
        };

        constexpr std::array<Axis, 4> Axes{{
            {0x90, false},
            {0x94, true},
            {0xA0, false},
            {0xA4, true},
        }};
        static_assert(Abi::DXGamepadFirstButtonOffset + Abi::DXGamepadButtonCount
            == spDXGamepad::CapturedPcPrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool spDXGamepad::TryReadPcCachedValueForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
        std::uint32_t selector, std::uint32_t& output) noexcept
    {
        if (ReadWord(prefix.data()) != Abi::DXGamepadPrimary
            || ReadWord(prefix.data() + Abi::DXGamepadInterfaceOffset) != Abi::DXGamepadInterface)
            return false;

        std::uint32_t value = 0;
        if (prefix[Abi::DXGamepadActiveOffset] != 0)
        {
            const auto axis = selector - Abi::DXGamepadFirstAxisSelector;
            const auto button = selector - Abi::DXGamepadFirstButtonSelector;
            if (axis < Axes.size())
            {
                const auto& route = Axes[axis];
                const auto cached = ReadWord(prefix.data() + route.offset);
                value = route.negate ? 0u - cached : cached;
            }
            else if (button < Abi::DXGamepadButtonCount)
            {
                value = prefix[Abi::DXGamepadFirstButtonOffset + button] != 0;
            }
        }
        output = value;
        return true;
    }
} // namespace sparkplug::reconstruction
