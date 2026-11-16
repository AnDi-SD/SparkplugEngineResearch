#pragma once
#include "WorldStateIpc.h"

namespace winxcoop::world_state
{
    // Clock/state policy only. The bridge must independently retain and check
    // the exact admitted PID/creation owner, including on unreadable slots.
    class ActorLeasePolicy final
    {
        Role role_;
        std::uint32_t readyMilliseconds_;
        std::uint64_t initialStarted_ = 0;
        bool initializing_ = false, readySeen_ = false;
    public:
        ActorLeasePolicy(Role role, std::uint32_t readyMilliseconds) noexcept
            : role_(role), readyMilliseconds_(readyMilliseconds) {}

        [[nodiscard]] Terminal Check(std::uint64_t now, const ActorStatus& actor) const noexcept
        {
            if(actor.observedAtMilliseconds > now) return Terminal::Clock;
            if(initializing_ && !readySeen_ && now - initialStarted_ > InitialNativeApplyMilliseconds)
                return Terminal::ActorLeaseExpired;
            if(actor.availability == static_cast<unsigned>(Availability::Ready) &&
                now - actor.observedAtMilliseconds > readyMilliseconds_) return Terminal::ActorLeaseExpired;
            return Terminal::None;
        }

        [[nodiscard]] Terminal Observe(std::uint64_t now, const ActorStatus& actor) noexcept
        {
            const auto availability = static_cast<Availability>(actor.availability);
            if(availability == Availability::Initializing)
            {
                if(role_ != Role::Guest || readySeen_) return Terminal::Identity;
                if(actor.appliedEpoch || actor.appliedRevision) return Terminal::NativeApply;
                if(!initializing_) { initializing_ = true; initialStarted_ = actor.observedAtMilliseconds; }
            }
            const auto failure = Check(now, actor);
            if(failure != Terminal::None) return failure;
            if(availability == Availability::Ready)
            {
                if(initializing_ && !readySeen_ && (!actor.appliedEpoch || !actor.appliedRevision))
                    return Terminal::NativeApply;
                readySeen_ = true;
            }
            return Terminal::None;
        }
    };
}
