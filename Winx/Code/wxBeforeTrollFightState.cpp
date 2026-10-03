#include "wxBeforeTrollFightState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxBeforeTrollFightState>(); }
        const spRTTIRecord record{wxBeforeTrollFightState::ClassID, wxCharacterState::ClassID,
            "wxBeforeTrollFightState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxBeforeTrollFightState::wxBeforeTrollFightState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    wxBeforeTrollFightState::~wxBeforeTrollFightState()
    {
        // PC51F9A0 / PS230D750: no once-flag guard, no release of pending.
        // A disconnected analytical allocation has no native lifetime yet.
        if (lifetimeHost_)
        {
            lifetimeHost_->RemoveCompletionSubscriberForAnalysis(lifetimeConsumer_, *this);
            lifetimeHost_->UnsubscribeForAnalysis(5, *this);
        }
    }
    void wxBeforeTrollFightState::BindForAnalysis(void* owner, void* consumer, wxBeforeTrollFightStateHost& host)
    {
        if (lifetimeHost_) throw std::logic_error("BeforeTrollFight lifetime is already bound");
        if (!owner || !consumer) throw std::logic_error("BeforeTrollFight requires owner and consumer bindings");
        SetBindingsForAnalysis(owner, consumer, &host);
        lifetimeHost_ = &host;
        lifetimeConsumer_ = consumer;
    }
    wxBeforeTrollFightStateHost& wxBeforeTrollFightState::RequireBoundHost() const
    {
        if (!lifetimeHost_ || &RequireHostForAnalysis() != lifetimeHost_
            || GetCompletionConsumerForAnalysis() != lifetimeConsumer_)
            throw std::logic_error("BeforeTrollFight requires an intact explicit lifetime binding");
        return *lifetimeHost_;
    }
    const spRTTIRecord& wxBeforeTrollFightState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxBeforeTrollFightState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxBeforeTrollFightState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBeforeTrollFightState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void* wxBeforeTrollFightState::ChooseHandle(wxBeforeTrollFightStateHost& host)
    {
        const auto random = host.RandomWordForAnalysis();
        // uint32->x87 on PC, uint32->single on PS2. The latter rounds the
        // half-ULP boundary 7FFFFFC0 to 80000000 (nearest, ties to even).
        const auto threshold = host.ProfileForAnalysis() == wxBeforeTrollProfileForAnalysis::PS2
            ? 0x7FFFFFC0u : 0x80000000u;
        return random < threshold ? handle44_ : handle48_;
    }
    void wxBeforeTrollFightState::SetDeadline(wxBeforeTrollFightStateHost& host)
    {
        const auto clock = host.ClockWordForAnalysis();
        const auto random = host.RandomWordForAnalysis();
        deadline_ = clock + 1000u + random % 1501u;
    }
    bool wxBeforeTrollFightState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = RequireBoundHost();
        // PC51FB30 / PS230D220: reload all three handles on every entry.
        handle44_ = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), 0);
        handle48_ = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), 0x800000);
        handle40_ = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), 0x8000);
        void* const chosen = ChooseHandle(host);
        if (chosen != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState();
            QueuePendingFromState(chosen, true, true);
            SetPendingHandleFromState(chosen);
            SetDeadline(host);
        }
        // Native base entry releases the freshly selected pending again.
        return wxCharacterState::vfunc_1C(request);
    }
    void wxBeforeTrollFightState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        auto& host = RequireBoundHost();
        if (GetTransitionFlag1D())
        {
            host.AddCompletionSubscriberForAnalysis(GetCompletionConsumerForAnalysis(), *this);
            host.SubscribeForAnalysis(5, *this);
            ClearTransitionFlag1D();
        }
        if (deadline_ && deadline_ < host.ClockWordForAnalysis())
        {
            if (handle40_ != GetPendingHandleForAnalysis())
            {
                ReleasePendingFromState();
                QueuePendingFromState(handle40_, false, true);
                SetPendingHandleFromState(handle40_);
                deadline_ = 0;
            }
        }
    }
    bool wxBeforeTrollFightState::vfunc_34(std::uint32_t) { return false; }
    void wxBeforeTrollFightState::vfunc_0C(const void* notification) noexcept
    {
        const auto& packet = *static_cast<const wxBeforeTrollNotificationForAnalysis*>(notification);
        if (packet.code == 3)
        {
            if (packet.handle1C != handle40_) return;
            auto& host = RequireBoundHost();
            void* const chosen = ChooseHandle(host);
            if (chosen == GetPendingHandleForAnalysis()) return;
            QueuePendingFromState(chosen, true, true);
            SetPendingHandleFromState(chosen);
            SetDeadline(host);
        }
        else if (packet.code == 0x272A && (packet.word18 & 0xFFu))
        {
            auto& host = RequireBoundHost();
            const bool ps2 = host.ProfileForAnalysis() == wxBeforeTrollProfileForAnalysis::PS2;
            ReleasePendingFromState();
            host.InvokeNodeForAnalysis(host.OwnerNodeForAnalysis(GetOwnerForAnalysis()), 0, 1);
            void* const node = host.OwnerNodeForAnalysis(GetOwnerForAnalysis());
            if (host.NodeHasChildrenForAnalysis(node))
            {
                void* child = host.FirstNodeChildForAnalysis(ps2 ? host.OwnerNodeForAnalysis(GetOwnerForAnalysis()) : node);
                if (child)
                {
                    // EE rereads owner/node and accesses element zero a second time.
                    if (ps2) child = host.FirstNodeChildForAnalysis(host.OwnerNodeForAnalysis(GetOwnerForAnalysis()));
                    host.WriteChildByteForAnalysis(child, ps2 ? 0x84u : 0x88u, 0);
                }
            }
            if (void* const object = host.OwnerResetObjectForAnalysis(GetOwnerForAnalysis()))
            {
                if (!ps2) host.ResetPCObjectForAnalysis(object);
                else
                    for (const auto offset : {0x1C8u, 0x1CCu, 0x1D0u, 0x1BCu, 0x1C0u, 0x1C4u,
                        0x1D4u, 0x1D8u, 0x1DCu, 0x1A0u, 0x1A4u, 0x1A8u, 0x194u, 0x198u, 0x19Cu})
                        host.WritePS2ResetWordForAnalysis(object, offset, 0);
            }
        }
    }
}
