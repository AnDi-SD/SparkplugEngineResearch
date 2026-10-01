#include "wxDialogueState.h"
#include "Analysis/Host/wxDialogueStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDialogueState()
        { return std::make_unique<wxDialogueState>(); }
        const spRTTIRecord record{wxDialogueState::ClassID, wxCharacterState::ClassID,
            "wxDialogueState", &wxCharacterState::StaticRTTI(), &CreateDialogueState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDialogueState::wxDialogueState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDialogueState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxDialogueState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDialogueState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDialogueState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDialogueState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxDialogueStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxDialogueState requires a dialogue-state host");
        host->SendEntryMessageForAnalysis(*this, 0x27BA, 0x0E, 0, 0);
        request.packedKey = (request.packedKey & 0xFFFFFFF8u) | 8u;
        return wxCharacterState::vfunc_1C(request);
    }
    std::uint32_t wxDialogueState::ComposeDialogueKeyForAnalysis(std::uint32_t input) noexcept
    {
        input = (input & 0xF007FF88u) | 8u;
        const auto action = (input >> 7) & 0xFFu;
        if (action < 28 || action > 57) input &= 0xFFFF8070u;
        return input;
    }
    void wxDialogueState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = ComposeDialogueKeyForAnalysis(request.packedKey);
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        // PC5A6AC4 / PS22CAF24 force a stop even when the owner predicate
        // would normally select fade. The later play still uses that predicate.
        ReleasePendingFromState(true);
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
    bool wxDialogueState::vfunc_34(std::uint32_t)
    {
        auto* host = dynamic_cast<wxDialogueStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxDialogueState requires a dialogue-state host");
        return host->GameStateForAnalysis() != 70;
    }
}
