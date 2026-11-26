#include "wxWinxHelpStateMachine.h"

#include "Analysis/PC/wxWinxHelpStateMachineAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxWinxHelpStateMachineRawCodeFieldOffset + sizeof(std::uint32_t)
            <= wxWinxHelpStateMachine::CapturedPrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxWinxHelpStateMachine::TryReadPcMaskedCodeForAnalysis(const CapturedPrefixForAnalysis& state,
        std::uint32_t, std::uint32_t& eax) noexcept
    {
        if (ReadWord(state.data()) != Abi::wxWinxHelpStateMachineVTable)
            return false;

        const auto raw = ReadWord(state.data() + Abi::wxWinxHelpStateMachineRawCodeFieldOffset);
        eax = (raw & 0x7F80u) == 0x80u ? 0x16u : 0u;
        return true;
    }
}
