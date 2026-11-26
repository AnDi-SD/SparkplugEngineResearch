#include "wxChestTrigger.h"

#include "Analysis/PC/wxChestTriggerAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxChestTriggerOwnedWordOffset + sizeof(std::uint32_t)
            <= wxChestTrigger::CapturedPrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxChestTrigger::TryReadPcOwnedWordIsZeroForAnalysis(
        const CapturedPrefixForAnalysis& state, std::uint32_t& eax) noexcept
    {
        if (ReadWord(state.data()) != Abi::wxChestTriggerVTable)
            return false;

        eax = ReadWord(state.data() + Abi::wxChestTriggerOwnedWordOffset) == 0u ? 1u : 0u;
        return true;
    }
}
