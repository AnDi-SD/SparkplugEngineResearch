#include "wxKnutAttackState.h"
#include "Analysis/Host/wxKnutAttackStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxKnutAttackState>(); }
        const spRTTIRecord record{wxKnutAttackState::ClassID, wxCharacterState::ClassID,
            "wxKnutAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxKnutAttackStateHost& RequireKnutHost(wxCharacterStateHost& host)
        {
            auto* knut = dynamic_cast<wxKnutAttackStateHost*>(&host);
            if (!knut) throw std::logic_error("wxKnutAttackState requires a motion/event/controller host");
            return *knut;
        }
    }
    wxKnutAttackState::wxKnutAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxKnutAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxKnutAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxKnutAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxKnutAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxKnutAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5182F0 / PS22E79A0. The once flag clears after playback, including
        // equal handles; subsequent updates do not touch key or external objects.
        if (!GetTransitionFlag1D()) return;
        auto& host = RequireKnutHost(RequireHostForAnalysis());
        request.packedKey &= 0xFF987F8Fu;
        const bool variant = host.OwnerEntityMotionFlag1FForAnalysis(GetOwnerForAnalysis()) != 0;
        request.packedKey = variant ? (request.packedKey & 0xF0FFFFFFu) | 0x800000u
            : request.packedKey & 0xF07FFFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState();
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
        }
        ClearTransitionFlag1D();
    }
    bool wxKnutAttackState::vfunc_34(const std::uint32_t code)
    {
        // PC520E20 / PS22E82E0: code A and null pending bypass the query.
        if (code == 0xA || !GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxKnutAttackState::vfunc_3C(const void* event)
    {
        // PC518020 / PS22E7B00. First eleven matches are exact; final three
        // use substring searches, in the original precedence order.
        auto& host = RequireKnutHost(RequireHostForAnalysis());
        const char* const name = host.EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxKnutAttackState host returned a null event tag name");
        using Word = wxKnutNotificationWordForAnalysis;
        const auto notifyOwner = [&](std::uint32_t code, Word first, Word second)
        {
            void* const receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis());
            if (receiver) host.SendNotificationForAnalysis(receiver, *this, code, first, second);
        };
        struct ExactEvent { const char* name; std::uint32_t code; Word first; Word second; };
        static constexpr ExactEvent events[] = {
            {"event_turnleft_begin", 0x273C, {1, 0xFF}, {0, 0xFF}},
            {"event_turnleft_end", 0x273C, {0, 0xFF}, {0, 0xFF}},
            {"event_turnright_begin", 0x273C, {1, 0xFF}, {1, 0xFF}},
            {"event_turnright_end", 0x273C, {0, 0xFF}, {1, 0xFF}},
            {"event_turn_begin", 0x273C, {1, 0xFF}, {0, 0xFF}},
            {"event_turn_end", 0x273C, {0, 0xFF}, {0, 0xFF}},
            {"event_defense_begin", 0x2744, {1, 0xFF}, {0, 0xFFFFFFFF}},
            {"event_defense_end", 0x2744, {0, 0xFF}, {0, 0xFFFFFFFF}},
            {"event_charge_begin", 0x2745, {1, 0xFF}, {0, 0xFFFFFFFF}}
        };
        for (const auto& item : events)
            if (std::strcmp(name, item.name) == 0)
            {
                notifyOwner(item.code, item.first, item.second);
                return;
            }
        const bool hurtBegin = std::strcmp(name, "event_hurt_begin") == 0;
        if (hurtBegin || std::strcmp(name, "event_hurt_end") == 0)
        {
            notifyOwner(0x2746, {hurtBegin ? 1u : 0u, 0xFF}, {0, 0xFFFFFFFF});
            void* const receiver = host.GameCoreField2B4ForAnalysis();
            if (receiver) host.SendNotificationForAnalysis(receiver, *this, 0x274C,
                {hurtBegin ? 1u : 0u, 0xFF}, {hurtBegin ? 3u : 0u, 0xFFFFFFFF});
            return;
        }
        if (std::strstr(name, "begin"))
        {
            host.InvokeControllerSlot38ForAnalysis(host.OwnerEntityControllerForAnalysis(GetOwnerForAnalysis()), 1);
            notifyOwner(0x27D1, {12, 0xFFFFFFFF}, {0, 0xFFFFFFFF});
            return;
        }
        if (std::strstr(name, "scepter_start"))
            host.InvokeControllerSlot3CForAnalysis(host.OwnerEntityControllerForAnalysis(GetOwnerForAnalysis()), 1, 0);
        else if (std::strstr(name, "scepter_end"))
            host.InvokeControllerSlot3CForAnalysis(host.OwnerEntityControllerForAnalysis(GetOwnerForAnalysis()), 0, 0);
    }
}
