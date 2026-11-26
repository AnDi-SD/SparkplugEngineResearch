#include "wxShadowBeastAttackState.h"

#include "Analysis/PC/wxShadowBeastAttackStateAbi.h"
#include "wxAnimationControllerCapturedMarksForAnalysis.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        namespace Marks = pc_animation_controller_marks;

        static_assert(Abi::wxShadowBeastAttackStateCaptureSize
            == wxShadowBeastAttackState::CapturedStatePrefixForAnalysis{}.size());

        bool HasDisjointCapturedRanges(std::uint32_t stateAddress, std::uint32_t controllerAddress) noexcept
        {
            constexpr auto addressSpaceEnd = std::uint64_t{1} << 32;
            const auto stateEnd = std::uint64_t{stateAddress} + Abi::wxShadowBeastAttackStateCaptureSize;
            const auto controllerEnd = std::uint64_t{controllerAddress} + Marks::CapturedPrefix{}.size();
            return stateAddress != 0 && controllerAddress != 0
                && stateEnd <= addressSpaceEnd && controllerEnd <= addressSpaceEnd
                && (stateEnd <= controllerAddress || controllerEnd <= stateAddress);
        }
    }

    std::optional<std::uint8_t> wxShadowBeastAttackState::TryEvaluatePcPermissionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        std::uint32_t capturedStateAddress, const BorrowedControllerForAnalysis& controller) noexcept
    {
        if (Marks::ReadWord(state.data()) != Abi::wxShadowBeastAttackStateVTable)
            return std::nullopt;
        if (rawTarget == 10)
            return std::uint8_t{1};

        const auto handle = Marks::ReadWord(state.data() + Abi::wxShadowBeastAttackStatePendingOffset);
        const auto controllerAddress = Marks::ReadWord(state.data() + Abi::wxShadowBeastAttackStateConsumerOffset);
        if (controllerAddress != controller.capturedAddress || controller.prefix == nullptr
            || !HasDisjointCapturedRanges(capturedStateAddress, controllerAddress)
            || Marks::ReadWord(controller.prefix->data()) != Abi::wxAnimationControllerVtable)
            return std::nullopt;

        return Marks::Consume(*controller.prefix, handle);
    }
}
