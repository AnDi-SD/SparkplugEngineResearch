#include "wxMinotaurMovingState.h"
#include "Analysis/Host/wxMinotaurMovingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMinotaurMovingState>(); }
        const spRTTIRecord record{wxMinotaurMovingState::ClassID, wxCharacterState::ClassID,
            "wxMinotaurMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMinotaurMovingState::wxMinotaurMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMinotaurMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMinotaurMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMinotaurMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMinotaurMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMinotaurMovingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC521980 / PS23053A0: byte clear before the first virtual update.
        field3C_ = 0;
        vfunc_30(request);
        return wxCharacterState::vfunc_1C(request);
    }
    void wxMinotaurMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5219C0 / PS2305130. Preserve the original two thresholds and
        // the PC second load from the originally captured control pointer.
        auto* host = dynamic_cast<wxMinotaurMovingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxMinotaurMovingState requires a control-motion host");
        void* const control = host->OwnerControlObjectForAnalysis(GetOwnerForAnalysis());
        const float first = host->ReadControlMotionForAnalysis(control);
        if (first < 0.05f)
        {
            request.packedKey &= 0xFFFFFF8Fu;
            ClearOwnerActionControlFromState();
        }
        else
        {
            const float second = host->UsePS2FiniteMotionProfileForAnalysis() ? first
                : host->ReadControlMotionForAnalysis(control);
            if (second < 0.15f)
            {
                host->WriteControlWordForAnalysis(control, 0x3DCCCCCD);
                request.packedKey = (request.packedKey & 0xFFFFFFCFu) | 0x40u;
            }
            else request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        }
        request.packedKey &= 0xF0000070u;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        if (field3C_)
            field3C_ = static_cast<std::uint8_t>(!host->IsPendingAnimationCompleteForAnalysis(
                GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true));
        if (!field3C_)
        {
            const bool modeZero = (request.packedKey & 0x70u) == 0x40u;
            if (modeZero) field3C_ = 1;
            ReleasePendingFromState();
            QueuePendingFromState(handle, !modeZero, true);
            SetPendingHandleFromState(handle);
            return;
        }
        host->WriteOwnerActionControlForAnalysis(GetOwnerForAnalysis(), 0x3DCCCCCD);
    }
    bool wxMinotaurMovingState::vfunc_34(std::uint32_t) { return field3C_ == 0; }
}
