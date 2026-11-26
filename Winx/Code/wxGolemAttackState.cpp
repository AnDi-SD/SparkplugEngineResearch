#include "wxGolemAttackState.h"

#include "Analysis/PC/wxCharacterAbi.h"
#include "Analysis/PC/wxGolemAttackStateAbi.h"

#include <algorithm>

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxGolemAttackStateCharacterLinkOffset + sizeof(std::uint32_t)
            <= wxGolemAttackState::CapturedStatePrefixForAnalysis{}.size());
        static_assert(Abi::wxGolemAttackStateCharacterRecordLinkOffset + sizeof(std::uint32_t)
            <= wxGolemAttackState::CapturedCharacterPrefixForAnalysis{}.size());
        static_assert(Abi::wxGolemAttackStateRecordClearWordOffset + sizeof(std::uint32_t)
            <= wxGolemAttackState::CapturedRecordPrefixForAnalysis{}.size());

        std::uint32_t ReadAddress(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxGolemAttackState::TryClearPcBorrowedRecordWordForAnalysis(
        const CapturedStatePrefixForAnalysis& state, const BorrowedCharacterForAnalysis& character,
        const BorrowedRecordForAnalysis& record) noexcept
    {
        if (ReadAddress(state.data()) != Abi::wxGolemAttackStateVTable)
            return false;
        const auto characterAddress = ReadAddress(state.data() + Abi::wxGolemAttackStateCharacterLinkOffset);
        if (characterAddress == 0 || characterAddress != character.capturedAddress || character.prefix == nullptr
            || ReadAddress(character.prefix->data()) != Abi::wxCharacterVTable)
            return false;
        const auto recordAddress = ReadAddress(character.prefix->data() + Abi::wxGolemAttackStateCharacterRecordLinkOffset);
        if (recordAddress == 0 || recordAddress != record.capturedAddress || record.prefix == nullptr)
            return false;

        std::fill_n(record.prefix->begin() + Abi::wxGolemAttackStateRecordClearWordOffset,
            sizeof(std::uint32_t), std::uint8_t{0});
        return true;
    }
}
