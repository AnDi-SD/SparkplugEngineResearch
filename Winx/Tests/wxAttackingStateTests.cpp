#include "Analysis/PC/wxAttackingStateAbi.h"
#include "Analysis/PS2/wxAttackingStateAbi.h"
#include "Code/wxAttackingState.h"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace winx::reconstruction;

    void Require(const bool value, const char* message)
    {
        if (!value)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void* Pointer(const std::uintptr_t value)
    {
        return reinterpret_cast<void*>(value);
    }

    struct Host final : wxAttackingStateHost
    {
        bool overrideActive = false;
        bool ownerPredicate = false;
        bool complete = false;
        bool orbImmediate = false;
        bool orbAllowed = false;
        float motion = 0.0f;
        float angle = 0.0f;
        std::uint32_t mode = 0;
        std::uint32_t actionCount = 0;
        std::uint32_t firstAction = 0;
        std::uint32_t snowballs = 0;
        std::uint32_t lastKey = 0;
        std::uint32_t eventCode = 0xFFFFFFFF;
        void* nextHandle = Pointer(0x300);
        std::string trace;

        void Event(const char* name)
        {
            if (!trace.empty()) trace += ',';
            trace += name;
        }
        bool IsAttackOverrideActiveForAnalysis() override { Event("override"); return overrideActive; }
        float GetAttackMotionForAnalysis(void*) override { Event("motion"); return motion; }
        float GetAttackAngleForAnalysis(void*) override { Event("angle"); return angle; }
        std::uint32_t GetOwnerModeForAnalysis(void*) override { Event("mode"); return mode; }
        std::uint32_t GetOwnerActionCountForAnalysis(void*) override { Event("count"); return actionCount; }
        std::uint32_t GetFirstOwnerActionForAnalysis(void*) override { Event("first"); return firstAction; }
        std::string_view GetEventNameForAnalysis(const void* event) override
        {
            Event("name"); return static_cast<const char*>(event);
        }
        bool IsOrbImmediateForAnalysis(void*) override { Event("orb-immediate"); return orbImmediate; }
        bool CanTriggerOrbForAnalysis(void*, float value) override
        {
            Require(value == 1.0f, "orb argument"); Event("orb-check"); return orbAllowed;
        }
        void TriggerEventForAnalysis(void*, std::uint32_t code) override
        {
            Event("trigger"); eventCode = code;
        }
        std::uint32_t GetSnowballCountForAnalysis() override { Event("snow-count"); return snowballs; }
        void SetSnowballCountForAnalysis(std::uint32_t count) override
        {
            Event("snow-set"); snowballs = count;
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override
        {
            Event("lookup"); lastKey = key; return nextHandle;
        }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return ownerPredicate; }
        void ResetCompletionForAnalysis(void*, void*) override { Event("reset"); }
        void StartAnimationForAnalysis(void*, void*, bool modeValue,
            std::uint32_t fade, bool interrupt) override
        {
            Require(fade == 0 && interrupt, "playback arguments");
            Event(modeValue ? "start-live" : "start-transition");
        }
        void StopAnimationForAnalysis(void*, void*) override { Event("stop"); }
        void FadeAnimationForAnalysis(void*, void*, float duration) override
        {
            Require(duration == 0.4f, "fade duration"); Event("fade");
        }
        bool IsPendingAnimationCompleteForAnalysis(void*, void*, bool flag) override
        {
            Require(flag, "consuming completion query"); Event("complete"); return complete;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("clear-control"); }
    };
}

int main()
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    Require(sizeof(winx::evidence::pc::wxAttackingStateLayout) == 0x40
        && sizeof(winx::evidence::ps2::wxAttackingStateLayout) == 0x40,
        "paired physical size");
    wxAttackingState state;
    Require(state.GetStateSelectorForAnalysis() == 5
        && state.IsExactly(wxAttackingState::ClassID)
        && state.IsKindOf(wxCharacterState::ClassID), "factory and RTTI");
    auto made = spRTTIManager::Instance().Create(wxAttackingState::ClassID);
    Require(dynamic_cast<wxAttackingState*>(made.get()) != nullptr, "registered factory");

    Host host;
    state.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
    for (std::uint32_t code = 0; code != 20; ++code)
    {
        const bool forbidden = code == 0 || code == 4 || code == 8
            || code == 10 || code == 18;
        Require(state.vfunc_38(code) == !forbidden, "PC/PS2 permission table");
    }

    host.orbAllowed = true;
    state.vfunc_3C("event_orb");
    Require(state.GetEventFlagForAnalysis() && host.eventCode == 0
        && host.trace == "name,orb-immediate,orb-check,trigger", "orb event path");
    Require(state.vfunc_34(1), "orb flag permission");
    host.mode = 3;
    Require(!state.vfunc_34(1) && state.vfunc_34(2), "mode-three exception");
    host.trace.clear();
    host.snowballs = 2;
    state.vfunc_3C("event_snowball");
    Require(host.snowballs == 1 && host.eventCode == 6
        && host.trace == "name,snow-count,trigger,snow-set", "snowball event ordering");
    state.vfunc_3C("other");
    Require(state.GetEventFlagForAnalysis(), "unrelated event leaves flag");

    host.trace.clear();
    wxAnimationRequestForAnalysis key{2};
    Require(!state.vfunc_1C(key) && !state.GetEventFlagForAnalysis(), "attack entry waits");
    Require(key.packedKey == 0x00200002
        && state.GetPendingHandleForAnalysis() == Pointer(0x300), "entry key and handle");
    Require(host.trace == "override,motion,override,lookup,reset,predicate,start-transition",
        "entry callback order");
    host.trace.clear();
    Require(!state.vfunc_1C(key) && host.trace == "override,complete", "incomplete entry");
    host.complete = true;
    host.motion = 1.0f;
    host.angle = 0.0f;
    host.nextHandle = Pointer(0x400);
    host.trace.clear();
    Require(state.vfunc_1C(key) && state.GetPendingHandleForAnalysis() == Pointer(0x400),
        "completed entry calls base and virtual active selection");
    Require(host.trace.find("complete,predicate,stop,motion,angle,override,lookup,predicate,start-live")
            != std::string::npos && (key.packedKey & 0x70) == 0x10,
        "active replacement order and angle mode");

    host.complete = false;
    host.motion = 0.0f;
    host.trace.clear();
    key.packedKey = 2;
    Require(!state.vfunc_20(key) && (key.packedKey & 0x00400000) != 0,
        "exit transition key");
    host.complete = true;
    Require(state.vfunc_20(key) && state.GetPendingHandleForAnalysis() == nullptr,
        "exit completion releases pending handle");

    // Distinct native slot-30 branches use the same final variant bits on
    // PC and PS2, including the narrow central angle and both outer sides.
    struct VariantCase { float motion; float angle; std::uint32_t input; std::uint32_t variant; };
    for (const VariantCase sample : {
        VariantCase{0.0f, 0.0f, 0x72, 0x00},
        VariantCase{1.0f, 0.0f, 0x02, 0x10},
        VariantCase{1.0f, 1.0f, 0x02, 0x40},
        VariantCase{1.0f, -1.0f, 0x02, 0x30},
        VariantCase{1.0f, 3.0f, 0x02, 0x20},
        VariantCase{1.0f, 0.0f, 0x03, 0x50},
    })
    {
        wxAttackingState variantState;
        Host variantHost;
        variantState.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &variantHost);
        variantHost.motion = sample.motion;
        variantHost.angle = sample.angle;
        wxAnimationRequestForAnalysis variantKey{sample.input};
        variantState.vfunc_30(variantKey);
        Require((variantKey.packedKey & 0x70) == sample.variant, "active variant selection");
        variantHost.trace.clear();
        variantState.vfunc_30(variantKey);
        Require(variantHost.trace.find("start-live") == std::string::npos
            && variantHost.trace.find("stop") == std::string::npos,
            "same handle is not replayed");
    }

    wxAttackingState overrideState;
    Host overrideHost;
    overrideState.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &overrideHost);
    overrideHost.overrideActive = true;
    wxAnimationRequestForAnalysis overrideKey{0x0F000002};
    Require(overrideState.vfunc_1C(overrideKey)
        && (overrideKey.packedKey & 0x0F800000) == 0x00800000,
        "global override enters base transition and rewrites mode bits");

    wxAttackingState cloneSource;
    Host cloneHost;
    cloneSource.SetBindingsForAnalysis(Pointer(1), Pointer(2), &cloneHost);
    cloneSource.vfunc_3C("event_orb");
    auto clone = cloneSource.Clone();
    auto* attackClone = dynamic_cast<wxAttackingState*>(clone.get());
    Require(attackClone && !attackClone->GetEventFlagForAnalysis()
        && attackClone->GetStateSelectorForAnalysis() == 5, "empty copy slot");
    bool missingHost = false;
    try { wxAnimationRequestForAnalysis request{2}; attackClone->vfunc_30(request); }
    catch (const std::logic_error&) { missingHost = true; }
    Require(missingHost, "missing external host is explicit");
    return EXIT_SUCCESS;
}
