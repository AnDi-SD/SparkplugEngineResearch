#include "Code/wxAudioEmitter.h"
#include "Analysis/PC/wxAudioEmitterAbi.h"
#include "Analysis/PS2/wxAudioEmitterAbi.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool okay, const char* label)
    {
        if (!okay) { std::cerr << label << '\n'; std::exit(1); }
    }
    struct Host final : wxAudioEmitterHost
    {
        std::int32_t gameState = 37;
        wxAudioEntryForAnalysis entry{1, 0, {}};
        std::uint32_t random = 0;
        int clonedResource = 0;
        unsigned initialized = 0, code30 = 0, activated = 0, other = 0;
        unsigned lookups = 0, randomCalls = 0, played = 0, special = 0, copied = 0;
        bool copyOkay = true;
        const void* lastSound = nullptr;
        std::string trace;
        void ConstructEntityForAnalysis(wxAudioEmitter&, bool value) override
        { Check(value, "entity constructor argument"); trace += "E"; }
        void ConstructAudioStateForAnalysis(wxAudioEmitter&) override { trace += "A"; }
        void DestroyAudioStateForAnalysis(wxAudioEmitter&) noexcept override { trace += "a"; }
        void DestroyEntityForAnalysis(wxAudioEmitter&) noexcept override { trace += "e"; }
        bool CopyEntityForAnalysis(const wxAudioEmitter&, wxAudioEmitter&, spCloneManager&) const override
        { return copyOkay; }
        void CopyOwnedResourceForAnalysis(const wxAudioEmitter& from, wxAudioEmitter& to) const override
        {
            ++const_cast<Host*>(this)->copied;
            to.GetStateForAnalysis().ownedResource = from.GetStateForAnalysis().ownedResource
                ? &const_cast<Host*>(this)->clonedResource : nullptr;
        }
        std::int32_t GameStateForAnalysis() const override { return gameState; }
        wxAudioEntryForAnalysis* FindEntryForAnalysis(wxAudioEmitter&, const char* name,
            std::uint32_t variant, std::uint32_t constant) override
        {
            Check(std::string(name) == "event_step" && variant == 255 && constant == 1000000,
                "lookup key");
            ++lookups; return &entry;
        }
        std::uint32_t RandomForAnalysis() override { ++randomCalls; return random; }
        void PlayKind10ForAnalysis(const void* sound) override
        { ++special; lastSound = sound; }
        void PlaySoundForAnalysis(const void* sound) override
        { ++played; lastSound = sound; }
        void InitializeForAnalysis(wxAudioEmitter&) noexcept override { ++initialized; }
        void HandleCode30ForAnalysis(wxAudioEmitter&) noexcept override { ++code30; }
        void SetAudioActiveForAnalysis(wxAudioEmitter&, bool active) noexcept override
        { Check(active, "activate argument"); ++activated; }
        void HandleOtherNotificationForAnalysis(wxAudioEmitter&, const void*) noexcept override { ++other; }
    };
}

int main()
{
    using winx::reconstruction::wxAudioEmitter;
    using namespace winx::reconstruction;
    Check(sizeof(winx::evidence::pc::wxAudioEmitterLayout) == 0x194, "PC size");
    Check(sizeof(winx::evidence::ps2::wxAudioEmitterLayout) == 0x1B0, "PS2 size");
    Check(wxAudioEmitter::StaticRTTI().classID == wxAudioEmitter::ClassID, "class ID");
    Check(wxAudioEmitter::CompareKeysForAnalysis("a", 2, "b", 1), "name order");
    Check(!wxAudioEmitter::CompareKeysForAnalysis("a", 1, "a", 0), "variant zero");
    Check(!wxAudioEmitter::CompareKeysForAnalysis("a", 1, "a", 10), "variant ten");
    Check(wxAudioEmitter::CompareKeysForAnalysis("a", 1, "a", 2), "variant order");
    Host host;
    {
        wxAudioEmitter emitter(host);
        Check(host.trace == "EA", "constructor order");
        std::uint32_t code = 0x2711;
        emitter.vfunc_0C(&code);
        Check(host.activated == 0, "inactive 2711");
        emitter.GetStateForAnalysis().flag5D = 1;
        emitter.vfunc_0C(&code);
        code = 0x1C; emitter.vfunc_0C(&code);
        code = 0x1E; emitter.vfunc_0C(&code);
        code = 0x2713; emitter.vfunc_0C(&code);
        Check(host.activated == 1 && host.initialized == 1 && host.code30 == 1
            && host.other == 1, "notification routing");
        int a = 1, b = 2, c = 3;
        host.entry.sounds = {&a, &b, &c};
        host.gameState = 38;
        emitter.DispatchTagForAnalysis("event_step", 255);
        Check(host.lookups == 0 && host.randomCalls == 0, "state gate");
        host.gameState = 70;
        host.random = 1;
        emitter.DispatchTagForAnalysis("event_step", 255);
        Check(host.lastSound == &b && host.entry.previousIndex == 1, "first choice");
        emitter.DispatchTagForAnalysis("event_step", 255);
        Check(host.lastSound == &c && host.entry.previousIndex == 2, "repeat advances");
        host.entry.sounds = {&a};
        emitter.DispatchTagForAnalysis("event_step", 255);
        Check(host.lastSound == &a && host.entry.previousIndex == 2 && host.randomCalls == 3,
            "single sound preserves previous index and consumes random");
        host.entry.kind = 10;
        emitter.DispatchTagForAnalysis("event_step", 255);
        Check(host.special == 1 && host.lastSound == &a && host.randomCalls == 3,
            "kind ten first sound");
        wxAudioEmitter target(host);
        spCloneManager manager;
        emitter.GetStateForAnalysis().ownedResource = &a;
        emitter.GetStateForAnalysis().flag5C = 2;
        emitter.GetStateForAnalysis().flag5D = 4;
        emitter.GetStateForAnalysis().words[0] = 7;
        target.GetStateForAnalysis().flag5C = 9;
        host.copyOkay = false;
        Check(!emitter.vfunc_14(target, manager) && host.copied == 0, "base copy rejection");
        host.copyOkay = true;
        Check(emitter.vfunc_14(target, manager), "copy accepted");
        Check(host.copied == 1 && target.GetStateForAnalysis().ownedResource == &host.clonedResource
            && target.GetStateForAnalysis().flag5C == 9
            && target.GetStateForAnalysis().flag5D == 4
            && target.GetStateForAnalysis().words[0] == 7, "copy own fields");
    }
    Check(host.trace == "EAEAaeae", "destructor order");
    return 0;
}
