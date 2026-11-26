#include "spDXKeyboard.h"

#include "../../Analysis/PC/spDXKeyboardAbi.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace Abi = sparkplug::evidence::pc;

        // Observed PC scan indices for the dense, known raw keys 0 through 142.
        constexpr std::array<std::uint8_t, 143> ScanByKey{
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
            13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
            37, 38, 39, 40, 42, 43, 44, 45, 46, 47, 48, 49,
            50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
            62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
            74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 87, 88,
            156, 157, 181, 183, 184, 197, 199, 200, 201, 203, 205, 207,
            208, 209, 210, 211, 100, 101, 102, 103, 104, 105, 106, 107,
            108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
            120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
            132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
        };
        constexpr std::uint8_t ExtraKnownKey = 176;

        static_assert(Abi::DXKeyboardCurrentCacheOffset + 256 <= Abi::DXKeyboardCapturedPrefixBytes);
        static_assert(Abi::DXKeyboardChangedCacheOffset
            + *std::max_element(ScanByKey.begin(), ScanByKey.end()) + 1 == Abi::DXKeyboardCapturedPrefixBytes);
        static_assert(Abi::DXKeyboardChangedCacheOffset + ExtraKnownKey < Abi::DXKeyboardCapturedPrefixBytes);
        static_assert(spDXKeyboard::CapturedPcPrefixForAnalysis{}.size() == Abi::DXKeyboardCapturedPrefixBytes);

        std::uint32_t ReadLittleEndianWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }

        bool TryReadPcCachedKey(const spDXKeyboard::CapturedPcPrefixForAnalysis& prefix,
            std::uint32_t rawKey, std::size_t cacheOffset, std::uint8_t& output) noexcept
        {
            if (ReadLittleEndianWord(prefix.data()) != Abi::DXKeyboardPrimary
                || ReadLittleEndianWord(prefix.data() + Abi::DXKeyboardInterfaceOffset) != Abi::DXKeyboardInterface)
                return false;
            if (rawKey >= ScanByKey.size() && rawKey != ExtraKnownKey)
                return false;

            std::uint8_t result = 0;
            if (prefix[Abi::DXKeyboardAcquiredOffset] != 0)
            {
                const auto scan = rawKey == ExtraKnownKey ? ExtraKnownKey : ScanByKey[rawKey];
                result = static_cast<std::uint8_t>(prefix[cacheOffset + scan] == 1);
            }
            output = result;
            return true;
        }
    }

    bool spDXKeyboard::TryReadPcCurrentKeyForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
        std::uint32_t rawKey, std::uint8_t& output) noexcept
    {
        return TryReadPcCachedKey(prefix, rawKey, Abi::DXKeyboardCurrentCacheOffset, output);
    }

    bool spDXKeyboard::TryReadPcChangedKeyForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
        std::uint32_t rawKey, std::uint8_t& output) noexcept
    {
        return TryReadPcCachedKey(prefix, rawKey, Abi::DXKeyboardChangedCacheOffset, output);
    }
}
