#include "wxFrogJumpingState.h"
#include "Analysis/Host/wxFrogJumpingStateHost.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxFrogJumpingState>(); }
        const spRTTIRecord record{wxFrogJumpingState::ClassID, wxCharacterState::ClassID,
            "wxFrogJumpingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFrogJumpingState::wxFrogJumpingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFrogJumpingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFrogJumpingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFrogJumpingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFrogJumpingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxFrogJumpingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC520F10 / PS22F4480: unconditional queue, no old-handle release,
        // no virtual update call, even when the selected handle is unchanged.
        field3C_ = std::uint8_t{0};
        request.packedKey = (request.packedKey & 0xF01F805Fu) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        return true;
    }
    void wxFrogJumpingState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        // PC520E50 / PS22F4450. Missing byte is a host-model error; the
        // native allocation's unspecified contents are not guessed as zero.
        if (!field3C_) throw std::logic_error("wxFrogJumpingState byte3C is unspecified before entry");
        if (*field3C_ == 0) ClearOwnerActionControlFromState();
    }
    bool wxFrogJumpingState::vfunc_34(std::uint32_t code)
    {
        // PC520E20 / PS22F4590: codeA and null pending bypass the query.
        if (code == 10 || GetPendingHandleForAnalysis() == nullptr) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxFrogJumpingState::vfunc_3C(const void* event)
    {
        auto* host = dynamic_cast<wxFrogJumpingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxFrogJumpingState requires an event host");
        const char* const name = host->EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxFrogJumpingState event tag is null");
        // Native strstr (PC MSVCR71 import6D932C / PS240B208): embedded
        // strings and suffixes match; begin takes precedence over end.
        if (std::strstr(name, "event_jump_begin"))
        {
            auto& control = host->OwnerEntityJumpControlForAnalysis(GetOwnerForAnalysis());
            control.velocity[1] = field40_;
            control.velocity[0] = 0;
            control.velocity[2] = 0;
            control.enabled = true;
            field3C_ = std::uint8_t{1};
        }
        else if (std::strstr(name, "event_jump_end"))
        {
            field3C_ = std::uint8_t{0};
        }
    }
}
