#include "wxIceGargoyleMovingState.h"
#include "Analysis/Host/wxIceGargoyleMovingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceGargoyleMovingState>(); }
        const spRTTIRecord record{wxIceGargoyleMovingState::ClassID, wxCharacterState::ClassID,
            "wxIceGargoyleMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxIceGargoyleMovingStateHost& RequireGargoyleHost(wxCharacterStateHost& base)
        {
            auto* host = dynamic_cast<wxIceGargoyleMovingStateHost*>(&base);
            if (!host) throw std::logic_error("wxIceGargoyleMovingState requires a gargoyle-moving host");
            return *host;
        }
    }
    wxIceGargoyleMovingState::wxIceGargoyleMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceGargoyleMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceGargoyleMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceGargoyleMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceGargoyleMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxIceGargoyleMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC 519830 / PS2 2EEDE0. The motion pointer is borrowed even when
        // action800 skips the scalar read. This object does not own it.
        auto& host = RequireGargoyleHost(RequireHostForAnalysis());
        request.packedKey = (request.packedKey & 0xFFFFFFF1u) | 1u;
        const auto action = host.ReadOwnerActionKeyForAnalysis(GetOwnerForAnalysis());
        if ((action & 0x7F80u) == 0x800u || host.ReadOwnerMotionForAnalysis(GetOwnerForAnalysis()) <= 0.0f)
            request.packedKey &= 0xFFFFFF8Fu;
        else
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        request.packedKey &= 0xF01FFFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
        // PS2 floating-point qualification covers normal finite values and
        // signed zeros. EE special/denormal values remain unqualified.
    }
    bool wxIceGargoyleMovingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC 5198E0 / PS2 2EEC40. Original names of action800 and flags
        // remain unknown; retain the packed bits and the once1E guard.
        auto& host = RequireGargoyleHost(RequireHostForAnalysis());
        const auto action = host.ReadOwnerActionKeyForAnalysis(GetOwnerForAnalysis());
        if ((action & 0x7F80u) != 0x800u)
            return wxCharacterState::vfunc_20(request);
        if (GetTransitionFlag1E())
        {
            request.packedKey &= 0xFFFFFF80u;
            request.packedKey = (request.packedKey & 0xF05FFFFFu) | 0x400000u;
            ReleasePendingFromState();
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(),
            GetPendingHandleForAnalysis(), true))
        {
            return wxCharacterState::vfunc_20(request);
        }
        ClearOwnerActionControlFromState();
        return false;
    }
}
