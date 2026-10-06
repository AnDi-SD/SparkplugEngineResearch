#include "wxIceWormAttackState.h"
#include "Analysis/Host/wxIceWormAttackStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceWormAttackState>(); }
        const spRTTIRecord Record{wxIceWormAttackState::ClassID, wxCharacterState::ClassID,
            "wxIceWormAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxIceWormAttackStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxIceWormAttackStateHost*>(&host);
            if (!typed) throw std::logic_error("wxIceWormAttackState requires an owner/event host");
            return *typed;
        }
        bool BypassTransition(const std::uint32_t key) noexcept
        {
            const auto selector = (key >> 7) & 0xffu;
            return selector == 8 || selector == 10;
        }
    }
    wxIceWormAttackState::wxIceWormAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceWormAttackState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxIceWormAttackState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxIceWormAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceWormAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxIceWormAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC519630 / PS22EC430: bypass checks the incoming key before flags.
        if (BypassTransition(request.packedKey)) return wxCharacterState::vfunc_1C(request);
        if (GetTransitionFlag1C())
        {
            auto& host = Host(RequireHostForAnalysis());
            if (void* receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
                host.SendAttackNotificationForAnalysis(receiver, *this, 0x27d1, 0x6f);
            request.packedKey = (request.packedKey & 0xf0387fdfu) | 0x200050u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1C();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        return wxCharacterState::vfunc_1C(request);
    }
    bool wxIceWormAttackState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC519700 / PS22EC250: first exit releases before rewriting the key.
        if (BypassTransition(request.packedKey)) return wxCharacterState::vfunc_20(request);
        if (GetTransitionFlag1E())
        {
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xf0587fdfu) | 0x400050u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        auto& host = Host(RequireHostForAnalysis());
        if (void* receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
            host.SendAttackNotificationForAnalysis(receiver, *this, 0x27d2, 0x6f);
        return wxCharacterState::vfunc_20(request);
    }
    void wxIceWormAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xff987fdfu) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        // PC5195EC reloads the caller's key after the foreign release callbacks.
        QueuePendingFromState(handle, !BypassTransition(request.packedKey), true);
        SetPendingHandleFromState(handle);
    }
    bool wxIceWormAttackState::vfunc_34(std::uint32_t)
    {
        const auto word = Host(RequireHostForAnalysis()).OwnerPackedWordForAnalysis(GetOwnerForAnalysis()) & 0x7f80u;
        if (word != 0x400 && word != 0x500) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxIceWormAttackState::vfunc_3C(const void* event)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (host.EventTagNameForAnalysis(event) != "event_shoot") return;
        if (void* receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
            host.SendAttackNotificationForAnalysis(receiver, *this, 0x2758, 0);
    }
}
