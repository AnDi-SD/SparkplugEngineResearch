#include "Code/wxDarcyAttackAIAction.h"
#include "Code/wxCharacterRegistry.h"
#include "Analysis/PC/wxDarcyAttackAIActionAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Profile = wxDarcyAttackAIAction::ProfileForAnalysis;
    using Result = wxDarcyAttackAIActionResultForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    std::array<std::uint8_t, 0xB4> Decode(const std::string& text)
    {
        Check(text.size() == 360, "Complete180-byte captured own rest"); std::array<std::uint8_t, 0xB4> result{};
        for (unsigned i = 0; i != result.size(); ++i) result[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    bool Same(const wxDarcyAttackAIActionCapturedStateForAnalysis& a, const wxDarcyAttackAIActionCapturedStateForAnalysis& b)
    { return a.borrowedTarget3A8 == b.borrowedTarget3A8 && a.ownBytes3AC == b.ownBytes3AC; }
    struct GroupInput { bool storage = true; std::vector<unsigned> targets; };
    struct Config
    {
        unsigned index = 0, cache = 0, outerStorage = 1, calls = 1, oldTarget = 1;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 0xB4> rest{};
        std::vector<GroupInput> groups;
        unsigned changeCache = 0, cacheAfter = 0, changeTarget = 0, targetAfter = 0, changeGlobal = 0, globalAfter = 1;
    };
    struct Fixture
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        unsigned owner = 123;
        std::vector<std::vector<void*>> targetStorage;
        std::vector<wxCharacterRegistry::GroupForAnalysis> groups;
        std::vector<const wxCharacterRegistry::GroupForAnalysis*> groupPointers;
        std::array<const wxCharacterRegistry::GroupForAnalysis*, 1> emptyOuter{{nullptr}};
        std::unique_ptr<wxCharacterRegistry> registry, replacement;
        wxCharacterRegistryBindingForAnalysis binding;
        std::unique_ptr<wxDarcyAttackAIAction> action;
        void* Target(unsigned id)
        { Check(id <= 2, "Live supplied target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Live target identity"); }
        explicit Fixture(const Config& c, bool registryAdmitted = true)
        {
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            targetStorage.reserve(c.groups.size()); groups.reserve(c.groups.size()); groupPointers.reserve(c.groups.size());
            for (const auto& input : c.groups)
            {
                targetStorage.emplace_back(input.targets.empty() ? 1 : input.targets.size(), nullptr);
                auto& storage = targetStorage.back(); for (unsigned i = 0; i != input.targets.size(); ++i) storage[i] = Target(input.targets[i]);
                groups.push_back({{input.storage ? storage.data() : nullptr, input.targets.size()}});
            }
            for (const auto& group : groups) groupPointers.push_back(&group);
            const auto* const* outer = groupPointers.empty() ? (c.outerStorage ? emptyOuter.data() : nullptr) : groupPointers.data();
            registry = std::make_unique<wxCharacterRegistry>(wxCharacterRegistry::CapturedStateForAnalysis{Target(c.cache), {outer, groupPointers.size()}},
                registryAdmitted ? wxCharacterRegistry::ProfileForAnalysis::PcExistingBorrowedVectors : wxCharacterRegistry::ProfileForAnalysis::Unqualified);
            replacement = std::make_unique<wxCharacterRegistry>(wxCharacterRegistry::CapturedStateForAnalysis{Target(2), {}}, wxCharacterRegistry::ProfileForAnalysis::PcExistingBorrowedVectors);
            binding.existing = registry.get();
            action = std::make_unique<wxDarcyAttackAIAction>(binding, wxDarcyAttackAIActionCapturedStateForAnalysis{Target(c.oldTarget), c.rest}, Profile::PcCapturedExistingRegistryEntry);
            action->SetOwnerForAnalysis(&owner); action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        ~Fixture() { action.reset(); }
        std::string Snapshot() const
        {
            Check(binding.existing == registry.get() || binding.existing == replacement.get(), "Live Registry binding identity");
            std::ostringstream out; const auto& state = action->CapturedStateForAnalysis();
            out << "{\"globalRegistry\":" << (binding.existing == registry.get() ? 1 : 2)
                << ",\"registryCaches\":[" << TargetID(registry->GetCapturedStateForAnalysis().cachedTarget) << ',' << TargetID(replacement->GetCapturedStateForAnalysis().cachedTarget)
                << "],\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"duration370\":" << action->GetDurationMillisecondsForAnalysis()
                << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    void One(const Config& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.cache <= 2 && c.outerStorage <= 1, "Bounded Darcy replay domain");
        Fixture f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeCache || c.changeTarget || c.changeGlobal))
            {
                const auto before = f.Snapshot();
                if (c.changeCache) f.registry->CapturedStateForFixture().cachedTarget = f.Target(c.cacheAfter);
                if (c.changeTarget) { Check(!f.targetStorage.empty() && !f.targetStorage[0].empty(), "Live first target slot for explicit input change"); f.targetStorage[0][0] = f.Target(c.targetAfter); }
                if (c.changeGlobal) { Check(c.globalAfter == 1 || c.globalAfter == 2, "Live replacement Registry"); f.binding.existing = c.globalAfter == 1 ? f.registry.get() : f.replacement.get(); }
                std::ostringstream out; out << "{\"call\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); f.action->vfunc_30();
            Check(f.action->LastResultForAnalysis() == Result::Completed, "Admitted whole Entry completes");
            Check(f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable
                && f.action->GetOwnerForAnalysis() == &f.owner, "Parent cells and untouched owner preserved");
            std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"explicitBetweenCallChanges\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; }
            std::cout << "]}\n";
        }
    }
    void Replay()
    {
        Config c; unsigned groupCount, storage, targetCount; std::string rest;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.cache >> c.outerStorage >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> groupCount), "Complete Darcy replay input");
            c.rest = Decode(rest); c.groups.clear(); Check(groupCount <= 4, "Bounded group count");
            for (unsigned i = 0; i != groupCount; ++i)
            {
                Check(bool(std::cin >> storage >> targetCount) && storage <= 1 && targetCount <= 4, "Bounded target record");
                GroupInput group; group.storage = bool(storage);
                for (unsigned j = 0, value; j != targetCount; ++j) { Check(bool(std::cin >> value) && value <= 2, "Live target replay ID"); group.targets.push_back(value); }
                c.groups.push_back(std::move(group));
            }
            Check(bool(std::cin >> c.changeCache >> c.cacheAfter >> c.changeTarget >> c.targetAfter >> c.changeGlobal >> c.globalAfter), "Complete between-call inputs"); One(c, true);
        }
        Check(std::cin.eof(), "Complete Darcy replay record boundary");
    }
    Config Base()
    { Config c; c.rest.fill(0xCC); c.groups.push_back({true, {2}}); return c; }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxDarcyAttackAIAction> && !std::is_move_constructible_v<wxDarcyAttackAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxDarcyAttackAIActionLayout) == 0x460);
        { auto c = Base(); c.calls = 2; c.changeCache = 1; c.cacheAfter = 0; c.changeTarget = 1; c.targetAfter = 1; One(c, false); }
        { auto c = Base(); c.groups[0].targets[0] = 0; One(c, false); }
        { auto c = Base(); c.cache = 1; c.calls = 2; c.changeGlobal = 1; c.globalAfter = 2; One(c, false); }
        {
            auto c = Base(); Fixture f(c); const auto state = f.action->CapturedStateForAnalysis();
            wxDarcyAttackAIAction unqualified(f.binding, state);
            const auto registryBefore = f.registry->LastResultForAnalysis();
            Check(unqualified.EnterForAnalysis() == Result::UnqualifiedProfile && Same(unqualified.CapturedStateForAnalysis(), state)
                && f.registry->LastResultForAnalysis() == registryBefore, "Default profile refuses before shared selection or own effects");
            f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->EnterForAnalysis() == Result::Completed && !f.action->GetOwnerForAnalysis(), "Entry does not dereference or require an owner");
        }
        for (bool missingRegistry : {false, true})
        {
            auto c = Base(); Fixture f(c, false); const auto before = f.action->CapturedStateForAnalysis();
            if (missingRegistry) f.binding.existing = nullptr;
            Check(f.action->EnterForAnalysis() == (missingRegistry ? Result::MissingExistingRegistry : Result::RegistryNotAdmitted)
                && Same(before, f.action->CapturedStateForAnalysis()), "Unavailable or unqualified Registry refuses before all own stores");
        }
        {
            auto c = Base(); Fixture f(c); std::array<const wxCharacterRegistry::GroupForAnalysis*, 1> missing{{nullptr}};
            const auto before = f.action->CapturedStateForAnalysis(); f.registry->CapturedStateForFixture().borrowedGroups = {missing.data(), 1};
            Check(f.action->EnterForAnalysis() == Result::RegistryNotAdmitted && Same(before, f.action->CapturedStateForAnalysis())
                && f.registry->LastResultForAnalysis() == wxCharacterRegistry::ResultForAnalysis::MissingFirstGroup, "Actual shared Registry refusal propagates without replacement target");
        }
        {
            auto c = Base(); Fixture f(c); const auto before = f.action->CapturedStateForAnalysis(); wxAIAction other;
            f.action->SetCurrentActionForAnalysis(&other);
            Check(f.action->EnterForAnalysis() == Result::UnqualifiedProfile && Same(before, f.action->CapturedStateForAnalysis()), "Current-action graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis() == Result::UnqualifiedProfile && Same(before, f.action->CapturedStateForAnalysis()), "Owned-action graph refuses");
        }
        {
            auto c = Base(); Fixture f(c); f.action->vfunc_30(); const auto before = f.action->CapturedStateForAnalysis(); f.binding.existing = nullptr;
            Check(f.action->EnterForAnalysis() == Result::MissingExistingRegistry && Same(before, f.action->CapturedStateForAnalysis()), "Null replacement binding refuses without stale Registry reuse");
        }
        {
            auto c = Base(); Fixture f(c); const auto before = f.Snapshot(); f.action->vfunc_0C(nullptr);
            Check(f.action->LastResultForAnalysis() == Result::UnqualifiedNotify, "Notify refuses");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Other bool hooks refuse");
            f.action->vfunc_34_ClearForAnalysis(); f.action->vfunc_38(); Check(f.action->LastResultForAnalysis() == Result::UnqualifiedHook, "Other void hooks report refusal");
            spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refuses"); bool refused = false;
            try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(wxDarcyAttackAIAction::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires captured bindings");
            Check(f.action->IsExactly(wxDarcyAttackAIAction::ClassID) && f.action->IsKindOf(wxAIAction::ClassID) && f.Snapshot() == before, "RTTI and unknown hooks preserve selected state");
            f.action->vfunc_30(); const auto targetBytes = f.targets; const auto cache = f.registry->GetCapturedStateForAnalysis().cachedTarget;
            f.action.reset();
            Check(f.binding.existing == f.registry.get() && f.targets == targetBytes && f.registry->GetCapturedStateForAnalysis().cachedTarget == cache
                && f.registry->SelectTargetForAnalysis().borrowedTarget == cache, "Borrowed Registry, binding and targets survive portable action teardown");
        }
    }
}
namespace
{
    using ExitResult = wxDarcyAttackAIActionExitResultForAnalysis;
    using ExitCommand = std::array<std::uint8_t, 116>;
    struct ExitActor { ExitCommand* command = nullptr; };
    struct ExitOwner { ExitActor* actor = nullptr; };
    struct ExitConfig
    {
        unsigned index = 0, calls = 1, oldTarget = 1, changeOwner = 0, changeActor = 0, changeCommand = 0;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 0xB4> rest{};
        ExitCommand command1{}, command2{};
    };
    ExitCommand DecodeExitCommand(const std::string& encoded)
    {
        Check(encoded.size() == 232, "Complete116-byte Exit command"); ExitCommand result{};
        for (unsigned i = 0; i != result.size(); ++i) result[i] = static_cast<std::uint8_t>(std::stoul(encoded.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    struct ExitFixture final : wxDarcyAttackAIActionExitHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<ExitCommand, 2> commands;
        std::array<ExitActor, 2> actors;
        std::array<ExitOwner, 2> owners;
        wxCharacterRegistryBindingForAnalysis registryBinding;
        std::unique_ptr<wxDarcyAttackAIAction> action;
        bool admitted = true;
        unsigned actorReads = 0, commandReads = 0, stores = 0, failActorRead = 0;
        wxDarcyAttackAIActionCapturedStateForAnalysis before{};
        explicit ExitFixture(const ExitConfig& c) : commands{{c.command1, c.command2}},
            actors{{{&commands[0]}, {&commands[1]}}}, owners{{{&actors[0]}, {&actors[1]}}}
        {
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            // Entry remains independently unqualified; only Exit is bound.
            action = std::make_unique<wxDarcyAttackAIAction>(registryBinding,
                wxDarcyAttackAIActionCapturedStateForAnalysis{Target(c.oldTarget), c.rest});
            action->SetOwnerForAnalysis(&owners[0]); action->SetDurationMillisecondsForAnalysis(c.duration);
            action->SetClearableWordForAnalysis(c.clearable); action->BindExitForAnalysis(this);
        }
        ~ExitFixture() override { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Exit target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Exit live target identity"); }
        unsigned OwnerID(const void* owner) const
        { for (unsigned i = 0; i != 2; ++i) if (owner == &owners[i]) return i + 1; throw std::runtime_error("Exit live owner identity"); }
        unsigned ActorID(const ExitActor* actor) const
        { for (unsigned i = 0; i != 2; ++i) if (actor == &actors[i]) return i + 1; throw std::runtime_error("Exit live actor identity"); }
        unsigned CommandID(const ExitCommand* command) const
        { for (unsigned i = 0; i != 2; ++i) if (command == &commands[i]) return i + 1; throw std::runtime_error("Exit live command identity"); }
        ProfileForAnalysis ExitProfileForAnalysis() const noexcept override
        { return admitted ? ProfileForAnalysis::PcCapturedStableCommand : ProfileForAnalysis::Unqualified; }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        {
            Check(owner == action->GetOwnerForAnalysis() && actorReads == stores && commandReads == stores,
                "Exit freshly reloads owner and actor before each store"); ++actorReads;
            if (failActorRead == actorReads) return std::nullopt;
            for (auto& record : owners) if (owner == &record) return static_cast<void*>(record.actor);
            return std::nullopt;
        }
        std::optional<void*> ReadCommand130ForAnalysis(void* actor) noexcept override
        {
            Check(actorReads == stores + 1 && commandReads == stores, "Exit fresh actor precedes command storage read"); ++commandReads;
            for (auto& record : actors) if (actor == &record) return static_cast<void*>(record.command);
            return std::nullopt;
        }
        void WriteCommandByteForAnalysis(void* command, std::uint32_t offset, std::uint8_t value) noexcept override
        {
            constexpr std::array<std::uint32_t, 6> offsets{0x1D, 0x20, 0x21, 0x22, 0x23, 0x24};
            Check(stores < 6 && actorReads == stores + 1 && commandReads == stores + 1 && offset == offsets[stores] && value == 0,
                "Exit source owns exact fresh-chain byte store order");
            Check(Same(before, action->CapturedStateForAnalysis()), "Exit own target and raw rest remain unchanged before all six stores");
            for (auto& record : commands) if (command == &record) { record[offset] = value; ++stores; return; }
            Check(false, "Exit writable borrowed command record");
        }
        void BeginCall() { actorReads = commandReads = stores = 0; before = action->CapturedStateForAnalysis(); }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"command1\":\"" << Hex(commands[0]) << "\",\"command2\":\"" << Hex(commands[1])
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerActorIDs\":[" << ActorID(owners[0].actor) << ',' << ActorID(owners[1].actor)
                << "],\"actorCommandIDs\":[" << CommandID(actors[0].command) << ',' << CommandID(actors[1].command)
                << "],\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    void ExitOne(const ExitConfig& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.changeOwner <= 2 && c.changeActor <= 2 && c.changeCommand <= 2, "Bounded Exit replay controls");
        ExitFixture f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeOwner || c.changeActor || c.changeCommand))
            {
                const auto before = f.Snapshot();
                if (c.changeOwner) f.action->SetOwnerForAnalysis(&f.owners[c.changeOwner - 1]);
                if (c.changeActor) f.owners[0].actor = &f.actors[c.changeActor - 1];
                if (c.changeCommand) f.actors[0].command = &f.commands[c.changeCommand - 1];
                std::ostringstream out; out << "{\"call\":1,\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); f.BeginCall(); f.action->vfunc_34_ClearForAnalysis();
            Check(f.action->LastExitResultForAnalysis() == ExitResult::Completed && f.actorReads == 6 && f.commandReads == 6 && f.stores == 6,
                "Whole Exit owns six fresh storage triples and final target clear");
            Check(f.action->CapturedStateForAnalysis().borrowedTarget3A8 == nullptr && f.action->CapturedStateForAnalysis().ownBytes3AC == c.rest
                && f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable, "Exit clears target only, preserving raw own rest and parent cells");
            std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
    }
    void ExitReplay()
    {
        ExitConfig c; std::string rest, a, b;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> a >> b >> c.changeOwner >> c.changeActor >> c.changeCommand), "Complete Exit replay row");
            c.rest = Decode(rest); c.command1 = DecodeExitCommand(a); c.command2 = DecodeExitCommand(b); ExitOne(c, true);
        }
        Check(std::cin.eof(), "Complete Exit replay input boundary");
    }
    ExitConfig ExitBase()
    { ExitConfig c; c.rest.fill(0xCC); c.command1.fill(0xA5); c.command2.fill(0x5A); return c; }
    void ExitContracts()
    {
        { auto c = ExitBase(); c.calls = 2; c.changeCommand = 2; ExitOne(c, false); }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); const auto before = f.action->CapturedStateForAnalysis();
            f.action->BindExitForAnalysis(nullptr); f.action->vfunc_34_ClearForAnalysis();
            Check(f.action->LastExitResultForAnalysis() == ExitResult::UnqualifiedProfile && f.action->LastResultForAnalysis() == Result::UnqualifiedHook
                && Same(before, f.action->CapturedStateForAnalysis()) && !f.actorReads, "Existing default Exit remains unqualified before effects");
            f.action->BindExitForAnalysis(&f); f.admitted = false;
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Exit host defaults require independent explicit admission");
            f.admitted = true;
            Check(f.action->EnterForAnalysis() == Result::UnqualifiedProfile, "Exit admission does not admit Entry");
            f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->ExitForAnalysis() == ExitResult::MissingOwner && Same(before, f.action->CapturedStateForAnalysis()) && !f.actorReads, "Null supplied owner is analytical refusal, not native fault handling");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.owners[0].actor = nullptr;
            Check(f.action->ExitForAnalysis() == ExitResult::MissingActorStorage && f.stores == 0 && f.commandReads == 0, "Missing actor refuses before command access");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.actors[0].command = nullptr;
            Check(f.action->ExitForAnalysis() == ExitResult::MissingCommandStorage && !f.stores, "Missing command refuses before byte store");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.failActorRead = 4; const auto before = f.action->CapturedStateForAnalysis();
            Check(f.action->ExitForAnalysis() == ExitResult::MissingActorStorage && f.stores == 3 && Same(before, f.action->CapturedStateForAnalysis()), "Later analytical refusal retains command prefix and old own target");
            auto expected = c.command1; expected[0x1D] = expected[0x20] = expected[0x21] = 0;
            Check(f.commands[0] == expected && f.commands[1] == c.command2, "Only completed command prefix survives refusal");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); wxAIAction child; f.BeginCall(); f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Current action graph refuses Exit");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Owned action graph refuses Exit");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); Check(f.action->ExitForAnalysis() == ExitResult::Completed, "Admitted Exit completes");
            const auto commands = f.commands; const auto targets = f.targets; const auto* actor = f.owners[0].actor;
            f.action.reset(); Check(f.commands == commands && f.targets == targets && f.owners[0].actor == actor, "Borrowed Exit records survive portable teardown");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay();
        else if (argc == 2 && std::string(argv[1]) == "--exit-replay") ExitReplay();
        else if (argc == 2 && std::string(argv[1]) == "--exit-contracts") { ExitContracts(); std::cout << "Darcy bounded captured Exit contracts passed\n"; }
        else { Contracts(); ExitContracts(); std::cout << "Darcy bounded existing-Registry Entry contracts passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "Darcy " << error.what() << '\n'; return 1; }
}
