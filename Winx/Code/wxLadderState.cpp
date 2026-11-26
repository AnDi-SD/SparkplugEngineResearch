#include "wxLadderState.h"

#include "Analysis/PC/wxCharacterAbi.h"
#include "Analysis/PC/wxLadderStateAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxLadderStateCharacterLinkOffset + sizeof(std::uint32_t)
            <= wxLadderState::CapturedStatePrefixForAnalysis{}.size());
        static_assert(Abi::wxLadderCharacterRecordLinkOffset + sizeof(std::uint32_t)
            == wxLadderState::CapturedCharacterPrefixForAnalysis{}.size());
        static_assert(Abi::wxLadderRecordResetByteOffset + 1
            == wxLadderState::CapturedRecordPrefixForAnalysis{}.size());

        std::uint32_t ReadAddress(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool wxLadderState::TryApplyPcTransitionPermissionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        const BorrowedCharacterForAnalysis& character, const BorrowedRecordForAnalysis& record,
        std::uint8_t& al) noexcept
    {
        if (ReadAddress(state.data()) != Abi::wxLadderStateVTable)
            return false;
        if (rawTarget != 39u)
        {
            al = 1;
            return true;
        }

        const auto characterAddress = ReadAddress(state.data() + Abi::wxLadderStateCharacterLinkOffset);
        if (characterAddress == 0 || characterAddress != character.capturedAddress || character.prefix == nullptr
            || ReadAddress(character.prefix->data()) != Abi::wxCharacterVTable)
            return false;
        const auto recordAddress = ReadAddress(character.prefix->data() + Abi::wxLadderCharacterRecordLinkOffset);
        if (recordAddress == 0 || recordAddress != record.capturedAddress || record.prefix == nullptr)
            return false;

        (*record.prefix)[Abi::wxLadderRecordResetByteOffset] = 0;
        al = 0;
        return true;
    }
}
