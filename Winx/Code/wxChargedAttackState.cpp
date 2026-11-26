#include "wxChargedAttackState.h"

#include "Analysis/PC/wxCharacterAbi.h"
#include "Analysis/PC/wxChargedAttackStateAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxChargedAttackStateOwnerOffset + sizeof(std::uint32_t)
            <= wxChargedAttackState::CapturedStatePrefixForAnalysis{}.size());
        static_assert(Abi::wxChargedAttackStateCharacterWordOffset + sizeof(std::uint32_t)
            == wxChargedAttackState::CapturedCharacterPrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxChargedAttackState::TryReadPcTransitionPermissionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        const BorrowedCharacterForAnalysis& character, std::uint8_t& al) noexcept
    {
        if (ReadWord(state.data()) != Abi::wxChargedAttackStateVTable)
            return false;

        if (rawTarget == 1u)
        {
            const auto address = ReadWord(state.data() + Abi::wxChargedAttackStateOwnerOffset);
            if (address == 0 || address != character.capturedAddress || character.prefix == nullptr
                || ReadWord(character.prefix->data()) != Abi::wxCharacterVTable)
                return false;
            const auto word = ReadWord(character.prefix->data() + Abi::wxChargedAttackStateCharacterWordOffset);
            al = static_cast<std::uint8_t>((word & 0xFu) != 3u);
        }
        else
            al = static_cast<std::uint8_t>(rawTarget != 27u && rawTarget != 29u && rawTarget != 32u);

        return true;
    }
}
