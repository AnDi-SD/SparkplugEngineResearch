#include "wxTrollAttackState.h"

#include "Analysis/PC/wxTrollAttackStateAbi.h"
#include "wxAnimationControllerCapturedMarksForAnalysis.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;
        namespace Marks = pc_animation_controller_marks;

        static_assert(Abi::wxTrollAttackStatePendingOffset + sizeof(std::uint32_t)
            <= wxTrollAttackState::CapturedStatePrefixForAnalysis{}.size());
        static_assert(sizeof(Abi::wxAnimationControllerLayout)
            == wxTrollAttackState::CapturedControllerPrefixForAnalysis{}.size());
    }

    std::optional<std::uint8_t> wxTrollAttackState::TryEvaluatePcCompletionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        const BorrowedControllerForAnalysis& controller) noexcept
    {
        if (Marks::ReadWord(state.data()) != Abi::wxTrollAttackStateVTable)
            return std::nullopt;
        if (rawTarget == 0x16 || rawTarget == 0x1C)
            return std::uint8_t{1};

        const auto handle = Marks::ReadWord(state.data() + Abi::wxTrollAttackStatePendingOffset);
        if (handle == 0)
            return std::uint8_t{1};

        const auto controllerAddress = Marks::ReadWord(state.data() + Abi::wxTrollAttackStateConsumerOffset);
        if (controllerAddress == 0 || controllerAddress != controller.capturedAddress || controller.prefix == nullptr
            || Marks::ReadWord(controller.prefix->data()) != Abi::wxAnimationControllerVtable)
            return std::nullopt;

        return Marks::Consume(*controller.prefix, handle);
    }
}
