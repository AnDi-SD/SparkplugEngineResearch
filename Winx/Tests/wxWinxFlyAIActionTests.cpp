#include "Code/wxWinxFlyAIAction.h"
#include "Code/wxCharacterRegistry.h"
#include "Analysis/PC/wxWinxFlyAIActionAbi.h"
#include <array>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    std::array<std::uint8_t, 0x14> Decode(const std::string& text)
    {
        Check(text.size() == 40, "Complete20-byte captured own rest"); std::array<std::uint8_t, 0x14> result{};
        for (unsigned i = 0; i != 20; ++i) result[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    struct GroupInput { bool storage = true; std::vector<unsigned> targets; };
    struct Config
    {
        unsigned index = 0, cache = 0, outerStorage = 1, command = 1, calls = 1, oldTarget = 1;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 0x14> rest{};
        std::vector<GroupInput> groups;
        unsigned changeCache = 0, cacheAfter = 0, changeTarget = 0, targetAfter = 0;
    };
    using Command = std::array<std::uint8_t, 0x74>;
    struct Actor { Command* command = nullptr; };
    struct Owner { Actor* actor = nullptr; };
    struct Host final : wxWinxFlyAIActionHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        Command command;
        Actor actor{&command}; Owner owner{&actor};
        wxCharacterRegistry* registry = nullptr;
        wxWinxFlyAIAction* action = nullptr;
        bool admitted = true, missingRegistry = false;
        unsigned commandReads = 0, commandWrites = 0, registryReads = 0;
        unsigned callCommandReads = 0, callCommandWrites = 0, callRegistryReads = 0;
        wxWinxFlyAIActionCapturedStateForAnalysis before{};
        Host() { targets[0].fill(0x4B); targets[1].fill(0x5C); command.fill(0xD7); }
        void* Target(unsigned id) { Check(id <= 2, "Live supplied target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Live target identity"); }
        ProfileForAnalysis EntryProfileForAnalysis() const noexcept override
        { return admitted ? ProfileForAnalysis::PcCapturedExistingRegistryEntry : ProfileForAnalysis::Unqualified; }
        bool OwnUnchanged() const
        { const auto& state = action->CapturedStateForAnalysis(); return state.borrowedTarget3A8 == before.borrowedTarget3A8 && state.ownBytes3AC == before.ownBytes3AC; }
        std::optional<void*> ReadCommandForAnalysis(void* capturedOwner) noexcept override
        {
            Check(capturedOwner == &owner && callCommandReads == 0 && callRegistryReads == 0 && OwnUnchanged(), "Capture owner/command before Registry or own stores");
            ++commandReads; ++callCommandReads;
            if (!owner.actor) return std::nullopt;
            return static_cast<void*>(owner.actor->command);
        }
        void WriteCommandWordForAnalysis(void* capturedCommand, std::uint32_t offset, std::uint32_t value) noexcept override
        {
            Check(capturedCommand == &command && offset == 4 && value == 0 && callCommandReads == 1 && callCommandWrites == 0 && callRegistryReads == 0 && OwnUnchanged(), "Captured nullable command store precedes Registry and own stores");
            ++commandWrites; ++callCommandWrites; for (unsigned i = 4; i != 8; ++i) command[i] = 0;
        }
        wxCharacterRegistry* ReadExistingRegistryForAnalysis() noexcept override
        {
            Check(callCommandReads == 1 && callRegistryReads == 0 && OwnUnchanged(), "Read existing common Registry after nullable command prefix");
            if (actor.command) Check(callCommandWrites == 1, "Nonnull command already written"); else Check(callCommandWrites == 0, "Null command skips write");
            ++registryReads; ++callRegistryReads; return missingRegistry ? nullptr : registry;
        }
        void BeginCall()
        { before = action->CapturedStateForAnalysis(); callCommandReads = callCommandWrites = callRegistryReads = 0; }
        std::string Snapshot() const
        {
            std::ostringstream out; const auto& state = action->CapturedStateForAnalysis();
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"registryCachedTargetID\":" << TargetID(registry->GetCapturedStateForAnalysis().cachedTarget)
                << ",\"commandBytes\":\"" << Hex(command) << "\",\"duration370\":" << action->GetDurationMillisecondsForAnalysis()
                << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << ",\"ownerStable\":" << (action->GetOwnerForAnalysis() == &owner ? "true" : "false") << '}';
            return out.str();
        }
    };
    struct Fixture
    {
        Host host;
        std::vector<std::vector<void*>> targetStorage;
        std::vector<wxCharacterRegistry::GroupForAnalysis> groups;
        std::vector<const wxCharacterRegistry::GroupForAnalysis*> groupPointers;
        std::array<const wxCharacterRegistry::GroupForAnalysis*, 1> emptyOuter{{nullptr}};
        std::unique_ptr<wxCharacterRegistry> registry;
        std::unique_ptr<wxWinxFlyAIAction> action;
        explicit Fixture(const Config& c, bool registryAdmitted = true)
        {
            host.actor.command = c.command ? &host.command : nullptr;
            targetStorage.reserve(c.groups.size()); groups.reserve(c.groups.size()); groupPointers.reserve(c.groups.size());
            for (const auto& input : c.groups)
            {
                targetStorage.emplace_back(input.targets.empty() ? 1 : input.targets.size(), nullptr);
                auto& storage = targetStorage.back(); for (unsigned i = 0; i != input.targets.size(); ++i) storage[i] = host.Target(input.targets[i]);
                groups.push_back({{input.storage ? storage.data() : nullptr, input.targets.size()}});
            }
            for (const auto& group : groups) groupPointers.push_back(&group);
            const auto* const* outer = groupPointers.empty() ? (c.outerStorage ? emptyOuter.data() : nullptr) : groupPointers.data();
            registry = std::make_unique<wxCharacterRegistry>(wxCharacterRegistry::CapturedStateForAnalysis{host.Target(c.cache), {outer, groupPointers.size()}},
                registryAdmitted ? wxCharacterRegistry::ProfileForAnalysis::PcExistingBorrowedVectors : wxCharacterRegistry::ProfileForAnalysis::Unqualified);
            host.registry = registry.get();
            action = std::make_unique<wxWinxFlyAIAction>(host, &host.owner, wxWinxFlyAIActionCapturedStateForAnalysis{host.Target(c.oldTarget), c.rest});
            host.action = action.get(); action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        ~Fixture() { action.reset(); host.action = nullptr; }
    };
    void One(const Config& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.cache <= 2 && c.command <= 1 && c.outerStorage <= 1, "Bounded Fly replay domain");
        Fixture f(c); std::vector<std::string> steps;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1)
            {
                if (c.changeCache) f.registry->CapturedStateForFixture().cachedTarget = f.host.Target(c.cacheAfter);
                if (c.changeTarget) { Check(!f.targetStorage.empty() && !f.targetStorage[0].empty(), "Live first target slot for explicit input change"); f.targetStorage[0][0] = f.host.Target(c.targetAfter); }
            }
            const auto before = f.host.Snapshot(); f.host.BeginCall(); f.action->vfunc_30();
            Check(f.action->LastResultForAnalysis() == wxWinxFlyAIActionResultForAnalysis::Completed, "Admitted whole Entry completes");
            Check(f.host.callCommandReads == 1 && f.host.callRegistryReads == 1 && f.host.callCommandWrites == c.command, "One command capture/nullable store and real Registry binding per call");
            const auto& rest = f.action->CapturedStateForAnalysis().ownBytes3AC;
            for (unsigned i = 0; i != 17; ++i) Check(rest[i] == 0, "Five own destinations zeroed");
            for (unsigned i = 17; i != 20; ++i) Check(rest[i] == c.rest[i], "Captured padding preserved");
            Check(f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable, "Parent fields preserved");
            std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.host.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"commandReads\":" << f.host.commandReads << ",\"commandWrites\":" << f.host.commandWrites << ",\"registryReads\":" << f.host.registryReads << "}\n";
        }
    }
    void Replay()
    {
        Config c; unsigned groupCount, storage, targetCount; std::string rest;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.cache >> c.outerStorage >> c.command >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> groupCount), "Complete Fly replay input");
            c.rest = Decode(rest); c.groups.clear(); Check(groupCount <= 4, "Bounded group count");
            for (unsigned i = 0; i != groupCount; ++i)
            {
                Check(bool(std::cin >> storage >> targetCount) && storage <= 1 && targetCount <= 4, "Bounded target record");
                GroupInput group; group.storage = bool(storage);
                for (unsigned j = 0, value; j != targetCount; ++j) { Check(bool(std::cin >> value) && value <= 2, "Live target replay ID"); group.targets.push_back(value); }
                c.groups.push_back(std::move(group));
            }
            Check(bool(std::cin >> c.changeCache >> c.cacheAfter >> c.changeTarget >> c.targetAfter), "Complete between-call inputs"); One(c, true);
        }
        Check(std::cin.eof(), "Complete Fly replay record boundary");
    }
    Config Base()
    { Config c; c.rest.fill(0xCC); c.groups.push_back({true, {2}}); return c; }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxWinxFlyAIAction> && !std::is_move_constructible_v<wxWinxFlyAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxWinxFlyAIActionLayout) == 0x3C0);
        { auto c = Base(); c.calls = 2; c.changeCache = 1; c.cacheAfter = 0; c.changeTarget = 1; c.targetAfter = 1; One(c, false); }
        { auto c = Base(); c.command = 0; c.cache = 0; c.groups[0].targets[0] = 0; One(c, false); }
        {
            auto c = Base(); Fixture f(c); const auto before = f.host.Snapshot(); f.host.admitted = false; f.host.BeginCall();
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::UnqualifiedProfile && f.host.Snapshot() == before && !f.host.commandReads, "Default profile refuses before effects");
            f.host.admitted = true; f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::MissingOwner && !f.host.commandReads, "Missing owner refuses before storage");
        }
        {
            auto c = Base(); Fixture f(c); f.host.owner.actor = nullptr; f.host.BeginCall(); const auto before = f.action->CapturedStateForAnalysis();
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::MissingActorOrCommandStorage && !f.host.commandWrites && !f.host.registryReads && f.action->CapturedStateForAnalysis().ownBytes3AC == before.ownBytes3AC, "Unavailable actor storage differs from admitted null command");
        }
        for (bool missingRegistry : {false, true})
        {
            auto c = Base(); Fixture f(c, false); f.host.missingRegistry = missingRegistry; f.host.BeginCall(); const auto before = f.action->CapturedStateForAnalysis();
            const auto wanted = missingRegistry ? wxWinxFlyAIActionResultForAnalysis::MissingExistingRegistry : wxWinxFlyAIActionResultForAnalysis::RegistryNotAdmitted;
            Check(f.action->EnterForAnalysis() == wanted && f.host.commandWrites == 1 && f.host.registryReads == 1, "Later Registry refusal retains command prefix");
            Check(f.action->CapturedStateForAnalysis().ownBytes3AC == before.ownBytes3AC && f.action->CapturedStateForAnalysis().borrowedTarget3A8 == before.borrowedTarget3A8, "Registry refusal precedes all own clears");
            for (unsigned i = 4; i != 8; ++i) Check(f.host.command[i] == 0, "Completed command prefix remains");
        }
        {
            auto c = Base(); Fixture f(c); std::array<const wxCharacterRegistry::GroupForAnalysis*, 1> missing{{nullptr}};
            f.registry->CapturedStateForFixture().borrowedGroups = {missing.data(), 1}; f.host.BeginCall();
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::RegistryNotAdmitted && f.registry->LastResultForAnalysis() == wxCharacterRegistry::ResultForAnalysis::MissingFirstGroup && f.host.commandWrites == 1, "Shared Registry algorithm refusal propagates without replacement target");
        }
        {
            auto c = Base(); Fixture f(c); wxAIAction other; f.action->SetCurrentActionForAnalysis(&other);
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::UnqualifiedProfile && !f.host.commandReads, "Current-action graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis() == wxWinxFlyAIActionResultForAnalysis::UnqualifiedProfile && !f.host.commandReads, "Owned-action graph refuses");
        }
        {
            auto c = Base(); Fixture f(c); const auto before = f.host.Snapshot(); f.action->vfunc_0C(nullptr);
            Check(f.action->LastResultForAnalysis() == wxWinxFlyAIActionResultForAnalysis::UnqualifiedNotify, "Notify refuses");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Other bool hooks refuse");
            f.action->vfunc_34_ClearForAnalysis(); f.action->vfunc_38(); Check(f.action->LastResultForAnalysis() == wxWinxFlyAIActionResultForAnalysis::UnqualifiedHook, "Other void hooks report refusal");
            spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refuses"); bool refused = false;
            try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(wxWinxFlyAIAction::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires capture bindings");
            Check(f.action->IsExactly(wxWinxFlyAIAction::ClassID) && f.action->IsKindOf(wxAIAction::ClassID) && f.host.Snapshot() == before && !f.host.commandReads, "RTTI and unknown hooks preserve selected state");
            f.host.BeginCall(); f.action->vfunc_30(); const auto command = f.host.command; const auto target = f.registry->GetCapturedStateForAnalysis().cachedTarget;
            f.action.reset(); f.host.action = nullptr;
            Check(f.host.command == command && f.registry->GetCapturedStateForAnalysis().cachedTarget == target && f.host.actor.command == &f.host.command, "Borrowed controls/Registry/target survive portable Fly teardown");
        }
    }
}
int main(int argc, char** argv)
{
    try { if (argc == 2 && std::string(argv[1]) == "--replay") Replay(); else { Contracts(); std::cout << "WinxFly bounded existing-Registry Entry contracts passed\n"; } return 0; }
    catch (const std::exception& error) { std::cerr << "WinxFly " << error.what() << '\n'; return 1; }
}
