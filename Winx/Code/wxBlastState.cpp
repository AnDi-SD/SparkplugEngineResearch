#include "wxBlastState.h"
#include "Analysis/Host/wxBlastStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxBlastState>(); }
        const spRTTIRecord Record{wxBlastState::ClassID, wxCharacterState::ClassID,
            "wxBlastState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        wxBlastStateHost& Host(wxCharacterStateHost& host)
        {
            auto* typed = dynamic_cast<wxBlastStateHost*>(&host);
            if (!typed) throw std::logic_error("wxBlastState requires an event/resource/notification host");
            return *typed;
        }
        void SecondaryNotification(wxBlastStateHost& host, wxCharacterState& source, std::uint32_t code)
        {
            if (!host.SecondaryCharacterForAnalysis()) return;
            // Native performs the singleton/character read again after the
            // first null check. A disappearing character violates its graph.
            void* const character = host.SecondaryCharacterForAnalysis();
            if (!character) throw std::logic_error("wxBlastState secondary character disappeared between original reads");
            if (void* receiver = host.CharacterField24ForAnalysis(character))
                host.SendDirectedNotificationForAnalysis(receiver, source, code, {0, 0});
        }
    }
    wxBlastState::wxBlastState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxBlastState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxBlastState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxBlastState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBlastState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxBlastState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC518C60 / PS22C7C50. Lookup can change the caller-owned key.
        auto& host = Host(RequireHostForAnalysis());
        request.packedKey = (request.packedKey & 0xff800081u) | 0x81u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        const auto variant = (request.packedKey >> 23) & 31;
        if (variant < 7)
        {
            if (void* receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis()))
                host.SendDirectedNotificationForAnalysis(receiver, *this, 0x2747, {variant, 0});
            SecondaryNotification(host, *this, 0x2780);
        }
        else
            host.SendFilteredNotificationForAnalysis(*this, 0x2780, 8, {0, 0});
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        vfunc_30(request);
        return true;
    }
    void wxBlastState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxBlastState::vfunc_34(std::uint32_t)
    {
        // Shared native PC5203D0 / PS22C8140.
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxBlastState::vfunc_3C(const void* event)
    {
        auto& host = Host(RequireHostForAnalysis());
        const auto name = host.EventTagNameForAnalysis(event);
        // PC import MSVCR71.strstr and PS240B208 search a substring here.
        if (name.find("event_blast") != std::string_view::npos)
        {
            const auto mode = host.GlobalModeForAnalysis();
            std::uint32_t action = 9;
            float amount = 40.0f;
            if (mode < 6)
            {
                // PC keeps EAX; PS2 reloads global mode before threshold4.
                const auto secondMode = host.PlatformForAnalysis() == wxBlastStatePlatformForAnalysis::PS2
                    ? host.GlobalModeForAnalysis() : mode;
                action = secondMode >= 4 ? 8u : 7u;
                amount = secondMode >= 4 ? 20.0f : 15.0f;
            }
            void* const resource = host.OwnerResourceObjectForAnalysis(GetOwnerForAnalysis());
            if (host.ConsumeResourceForAnalysis(resource, amount))
            {
                void* const actionObject = host.OwnerActionObjectForAnalysis(GetOwnerForAnalysis());
                host.TriggerActionForAnalysis(actionObject, action);
                SecondaryNotification(host, *this, 0x2783);
            }
            else if (void* receiver = host.MainReceiverForAnalysis())
                host.SendDirectedNotificationForAnalysis(receiver, *this, 0x27d1, {0xcc, 0});
        }
        else if (name == "event_dragon_fire")
            host.SendFilteredNotificationForAnalysis(*this, 0x274b, 8, {5, 0});
    }
}
