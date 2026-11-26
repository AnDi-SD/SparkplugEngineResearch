#include "wxDispelTrigger.h"

#include "Analysis/PC/wxDispelTriggerAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        using Prefix = wxDispelTrigger::CapturedPrefixForAnalysis;

        static_assert(Abi::wxDispelTriggerActiveByteOffset < Prefix{}.size());

        bool TryStoreActiveByte(Prefix& state, std::uint8_t value) noexcept
        {
            const auto primary = std::uint32_t(state[0]) | (std::uint32_t(state[1]) << 8)
                | (std::uint32_t(state[2]) << 16) | (std::uint32_t(state[3]) << 24);
            if (primary != Abi::wxDispelTriggerVTable)
                return false;

            state[Abi::wxDispelTriggerActiveByteOffset] = value;
            return true;
        }
    }

    bool wxDispelTrigger::TrySetPcActiveByteForAnalysis(CapturedPrefixForAnalysis& state) noexcept
    {
        return TryStoreActiveByte(state, 1);
    }

    bool wxDispelTrigger::TryClearPcActiveByteForAnalysis(CapturedPrefixForAnalysis& state) noexcept
    {
        return TryStoreActiveByte(state, 0);
    }
}
