#include "wxFastAttackState.h"

#include "../Analysis/PC/wxCharacterAbi.h"
#include "../Analysis/PC/wxFastAttackStateAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxFastAttackStateRightOffset + sizeof(std::uint32_t)
            == wxFastAttackState::CapturedStatePrefixForAnalysis{}.size());
        static_assert(Abi::wxFastAttackCharacterWordOffset + sizeof(std::uint32_t)
            == wxFastAttackState::CapturedCharacterPrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxFastAttackState::TryReadPcTransitionPermissionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        const BorrowedCharacterForAnalysis& character, std::uint8_t& output) noexcept
    {
        if (ReadWord(state.data()) != Abi::wxFastAttackStateVTable)
            return false;

        if (rawTarget == 1u)
        {
            const auto ownerAddress = ReadWord(state.data() + Abi::wxFastAttackStateOwnerOffset);
            if (ownerAddress == 0 || ownerAddress != character.capturedAddress || character.prefix == nullptr
                || ReadWord(character.prefix->data()) != Abi::wxCharacterVTable)
                return false;
            const auto characterWord = ReadWord(character.prefix->data() + Abi::wxFastAttackCharacterWordOffset);
            if ((characterWord & 0xFu) == 3u)
            {
                output = 0;
                return true;
            }
        }

        const auto left = ReadWord(state.data() + Abi::wxFastAttackStateLeftOffset);
        const auto right = ReadWord(state.data() + Abi::wxFastAttackStateRightOffset);
        output = static_cast<std::uint8_t>(left >= right);
        return true;
    }
}
