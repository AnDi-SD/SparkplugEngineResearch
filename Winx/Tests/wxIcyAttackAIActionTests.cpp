#include "Code/wxIcyAttackAIAction.h"
#include "Code/wxCharacterRegistry.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxIcyAttackAIActionAbi.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Profile = wxIcyAttackAIAction::ProfileForAnalysis;
    using Result = wxIcyAttackAIActionResultForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    std::array<std::uint8_t, 0xD4> Decode(const std::string& text)
    {
        Check(text.size() == 424, "Complete212-byte captured own rest"); std::array<std::uint8_t, 0xD4> result{};
        for (unsigned i = 0; i != result.size(); ++i) result[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    bool Same(const wxIcyAttackAIActionCapturedStateForAnalysis& a, const wxIcyAttackAIActionCapturedStateForAnalysis& b)
    { return a.borrowedTarget3A8 == b.borrowedTarget3A8 && a.ownBytes3AC == b.ownBytes3AC; }
    struct GroupInput { bool storage = true; std::vector<unsigned> targets; };
    struct Config
    {
        unsigned index = 0, cache = 0, outerStorage = 1, calls = 1, oldTarget = 1;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 0xD4> rest{};
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
        std::unique_ptr<wxIcyAttackAIAction> action;
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
            action = std::make_unique<wxIcyAttackAIAction>(binding, wxIcyAttackAIActionCapturedStateForAnalysis{Target(c.oldTarget), c.rest}, Profile::PcCapturedExistingRegistryEntry);
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
        Check(c.calls >= 1 && c.calls <= 2 && c.cache <= 2 && c.outerStorage <= 1, "Bounded Icy replay domain");
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
            Check(bool(std::cin >> c.cache >> c.outerStorage >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> groupCount), "Complete Icy replay input");
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
        Check(std::cin.eof(), "Complete Icy replay record boundary");
    }
    Config Base()
    { Config c; c.rest.fill(0xCC); c.groups.push_back({true, {2}}); return c; }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxIcyAttackAIAction> && !std::is_move_constructible_v<wxIcyAttackAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxIcyAttackAIActionLayout) == 0x480);
        { auto c = Base(); c.calls = 2; c.changeCache = 1; c.cacheAfter = 0; c.changeTarget = 1; c.targetAfter = 1; One(c, false); }
        { auto c = Base(); c.groups[0].targets[0] = 0; One(c, false); }
        { auto c = Base(); c.cache = 1; c.calls = 2; c.changeGlobal = 1; c.globalAfter = 2; One(c, false); }
        {
            auto c = Base(); Fixture f(c); const auto state = f.action->CapturedStateForAnalysis();
            wxIcyAttackAIAction unqualified(f.binding, state);
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
            refused = false; try { (void)spRTTIManager::Instance().Create(wxIcyAttackAIAction::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires captured bindings");
            Check(f.action->IsExactly(wxIcyAttackAIAction::ClassID) && f.action->IsKindOf(wxAIAction::ClassID) && f.Snapshot() == before, "RTTI and unknown hooks preserve selected state");
            f.action->vfunc_30(); const auto targetBytes = f.targets; const auto cache = f.registry->GetCapturedStateForAnalysis().cachedTarget;
            f.action.reset();
            Check(f.binding.existing == f.registry.get() && f.targets == targetBytes && f.registry->GetCapturedStateForAnalysis().cachedTarget == cache
                && f.registry->SelectTargetForAnalysis().borrowedTarget == cache, "Borrowed Registry, binding and targets survive portable action teardown");
        }
    }
}
namespace
{
    using ExitResult = wxIcyAttackAIActionExitResultForAnalysis;
    using Command = wxAICommandRecordForAnalysis;
    // Provided fixture view length only; no native profile class/extent claim.
    using ProvidedProfile = std::array<std::uint8_t, 0x1000>;
    struct ExitActor { Command* command = nullptr; };
    struct ExitOwner { ExitActor* actor = nullptr; };
    struct ExitConfig
    {
        unsigned index = 0, calls = 1, oldTarget = 1, ownerID = 1, profileID = 1;
        unsigned changeOwner = 0, changeActor = 0, changeCommand = 0, changeProfile = 0;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 0xD4> rest{};
        Command::RawBytesForAnalysis command1{}, command2{};
        ProvidedProfile profile1{}, profile2{};
    };
    template<std::size_t N> std::array<std::uint8_t, N> DecodeExit(const std::string& encoded)
    {
        Check(encoded.size() == 2 * N, "Complete captured Exit byte view"); std::array<std::uint8_t, N> result{};
        for (unsigned i = 0; i != N; ++i) result[i] = static_cast<std::uint8_t>(std::stoul(encoded.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    struct ExitFixture final : wxIcyAttackAIActionExitHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<std::unique_ptr<Command>, 2> commands;
        std::array<ExitActor, 2> actors{};
        std::array<ExitOwner, 2> owners{};
        std::array<ProvidedProfile, 2> profiles;
        ProvidedProfile* existingProfile = nullptr;
        Command* selectedCommand = nullptr;
        wxCharacterRegistryBindingForAnalysis registryBinding;
        std::unique_ptr<wxIcyAttackAIAction> action;
        bool admitted = true;
        unsigned actorReads = 0, commandReads = 0, profileReads = 0;
        wxIcyAttackAIActionCapturedStateForAnalysis before{};
        explicit ExitFixture(const ExitConfig& c, bool recordAdmitted = true) : profiles{{c.profile1, c.profile2}}
        {
            Check(c.ownerID >= 1 && c.ownerID <= 2 && c.profileID >= 1 && c.profileID <= 2, "Supplied live initial bindings");
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            const auto profile = recordAdmitted ? Command::ProfileForAnalysis::PcCaptured703090 : Command::ProfileForAnalysis::Unqualified;
            commands[0] = std::make_unique<Command>(c.command1, profile); commands[1] = std::make_unique<Command>(c.command2, profile);
            for (unsigned i = 0; i != 2; ++i) { actors[i].command = commands[i].get(); owners[i].actor = &actors[i]; }
            existingProfile = &profiles[c.profileID - 1];
            // Entry remains independently unqualified; only Exit is bound.
            action = std::make_unique<wxIcyAttackAIAction>(registryBinding, wxIcyAttackAIActionCapturedStateForAnalysis{Target(c.oldTarget), c.rest});
            action->SetOwnerForAnalysis(&owners[c.ownerID - 1]); action->SetDurationMillisecondsForAnalysis(c.duration);
            action->SetClearableWordForAnalysis(c.clearable); action->BindExitForAnalysis(this);
        }
        ~ExitFixture() override { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Exit target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Live Exit target identity"); }
        unsigned OwnerID(const void* owner) const
        { for (unsigned i = 0; i != 2; ++i) if (owner == &owners[i]) return i + 1; throw std::runtime_error("Live Exit owner identity"); }
        unsigned ActorID(const ExitActor* actor) const
        { for (unsigned i = 0; i != 2; ++i) if (actor == &actors[i]) return i + 1; throw std::runtime_error("Live Exit actor identity"); }
        unsigned CommandID(const Command* command) const
        { for (unsigned i = 0; i != 2; ++i) if (command == commands[i].get()) return i + 1; throw std::runtime_error("Live Exit command identity"); }
        unsigned ProfileID() const
        { for (unsigned i = 0; i != 2; ++i) if (existingProfile == &profiles[i]) return i + 1; throw std::runtime_error("Live provided profile identity"); }
        ProfileForAnalysis ExitProfileForAnalysis() const noexcept override
        { return admitted ? ProfileForAnalysis::PcCapturedStableCommandAndExistingProfile : ProfileForAnalysis::Unqualified; }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        {
            Check(owner == action->GetOwnerForAnalysis() && actorReads == 0 && commandReads == 0 && profileReads == 0
                && Same(before, action->CapturedStateForAnalysis()), "Capture owner/actor before command or profile effects"); ++actorReads;
            for (auto& record : owners) if (owner == &record) return static_cast<void*>(record.actor);
            return std::nullopt;
        }
        std::optional<Command*> ReadCommand130ForAnalysis(void* actor) noexcept override
        {
            Check(actorReads == 1 && commandReads == 0 && profileReads == 0 && Same(before, action->CapturedStateForAnalysis()), "Command storage follows actor capture"); ++commandReads;
            for (auto& record : actors) if (actor == &record) { selectedCommand = record.command; return selectedCommand; }
            return std::nullopt;
        }
        std::uint8_t* ReadExistingProfileByte510ForAnalysis() noexcept override
        {
            Check(actorReads == 1 && commandReads == 1 && profileReads == 0 && selectedCommand
                && selectedCommand->LastResultForAnalysis() == Command::ResultForAnalysis::Completed
                && Same(before, action->CapturedStateForAnalysis()), "Real common Reset completes before profile read and own target clear");
            ++profileReads; return existingProfile ? &(*existingProfile)[0x510] : nullptr;
        }
        void BeginCall() { actorReads = commandReads = profileReads = 0; selectedCommand = nullptr; before = action->CapturedStateForAnalysis(); }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"command1\":\"" << Hex(commands[0]->GetBytesForAnalysis()) << "\",\"command2\":\"" << Hex(commands[1]->GetBytesForAnalysis())
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerActorIDs\":[" << ActorID(owners[0].actor) << ',' << ActorID(owners[1].actor)
                << "],\"actorCommandIDs\":[" << CommandID(actors[0].command) << ',' << CommandID(actors[1].command)
                << "],\"profileBindingID\":" << ProfileID() << ",\"profile1\":\"" << Hex(profiles[0]) << "\",\"profile2\":\"" << Hex(profiles[1])
                << "\",\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    void ExitOne(const ExitConfig& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.changeOwner <= 2 && c.changeActor <= 2 && c.changeCommand <= 2 && c.changeProfile <= 2, "Bounded Exit inputs");
        ExitFixture f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeOwner || c.changeActor || c.changeCommand || c.changeProfile))
            {
                const auto before = f.Snapshot();
                if (c.changeOwner) f.action->SetOwnerForAnalysis(&f.owners[c.changeOwner - 1]);
                if (c.changeActor) f.owners[0].actor = &f.actors[c.changeActor - 1];
                if (c.changeCommand) f.actors[0].command = f.commands[c.changeCommand - 1].get();
                if (c.changeProfile) f.existingProfile = &f.profiles[c.changeProfile - 1];
                std::ostringstream out; out << "{\"call\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); f.BeginCall(); f.action->vfunc_34_ClearForAnalysis();
            Check(f.action->LastExitResultForAnalysis() == ExitResult::Completed && f.actorReads == 1 && f.commandReads == 1 && f.profileReads == 1,
                "Whole Exit resets actual record, then observes existing profile");
            Check(f.action->CapturedStateForAnalysis().borrowedTarget3A8 == nullptr && f.action->CapturedStateForAnalysis().ownBytes3AC == c.rest
                && f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable,
                "Exit preserves raw own tail and parent cells");
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
        ExitConfig c; std::string rest, a, b, p, q;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> a >> b >> p >> q >> c.ownerID >> c.profileID
                >> c.changeOwner >> c.changeActor >> c.changeCommand >> c.changeProfile), "Complete Exit replay input");
            c.rest = Decode(rest); c.command1 = DecodeExit<0x74>(a); c.command2 = DecodeExit<0x74>(b);
            c.profile1 = DecodeExit<0x1000>(p); c.profile2 = DecodeExit<0x1000>(q); ExitOne(c, true);
        }
        Check(std::cin.eof(), "Complete Exit replay boundary");
    }
    ExitConfig ExitBase()
    {
        ExitConfig c; c.rest.fill(0xCC); c.command1.fill(0xA5); c.command2.fill(0x5A); c.profile1.fill(0x17); c.profile2.fill(0xD7);
        for (auto* raw : {&c.command1, &c.command2}) { (*raw)[0] = 0x90; (*raw)[1] = 0x30; (*raw)[2] = 0x70; (*raw)[3] = 0; }
        return c;
    }
    void ExitContracts()
    {
        { auto c = ExitBase(); c.calls = 2; c.changeProfile = 2; ExitOne(c, false); }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); const auto before = f.action->CapturedStateForAnalysis();
            f.action->BindExitForAnalysis(nullptr); f.action->vfunc_34_ClearForAnalysis();
            Check(f.action->LastExitResultForAnalysis() == ExitResult::UnqualifiedProfile && f.action->LastResultForAnalysis() == Result::UnqualifiedHook
                && Same(before, f.action->CapturedStateForAnalysis()) && !f.actorReads, "Default Exit stays unbound before effects");
            f.action->BindExitForAnalysis(&f); f.admitted = false;
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Exit admission is explicit and separate");
            f.admitted = true; Check(f.action->EnterForAnalysis() == Result::UnqualifiedProfile, "Exit does not implicitly admit Entry");
            f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->ExitForAnalysis() == ExitResult::MissingOwner && !f.actorReads && f.commands[0]->GetBytesForAnalysis() == c.command1, "Missing owner refuses before record access");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.owners[0].actor = nullptr;
            Check(f.action->ExitForAnalysis() == ExitResult::MissingActorStorage && !f.commandReads && !f.profileReads, "Missing actor precedes command reset");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.actors[0].command = nullptr;
            Check(f.action->ExitForAnalysis() == ExitResult::MissingCommandStorage && !f.profileReads && f.commands[0]->GetBytesForAnalysis() == c.command1, "Missing command precedes reset and profile");
        }
        for (bool wrongTag : {false, true})
        {
            const auto c = ExitBase(); ExitFixture f(c, wrongTag); f.BeginCall(); const auto before = f.action->CapturedStateForAnalysis();
            if (wrongTag) f.commands[0]->BytesForFixture()[0] = 0x91; const auto raw = f.commands[0]->GetBytesForAnalysis();
            Check(f.action->ExitForAnalysis() == ExitResult::CommandNotAdmitted && !f.profileReads && Same(before, f.action->CapturedStateForAnalysis())
                && f.commands[0]->GetBytesForAnalysis() == raw && f.profiles[0] == c.profile1, "Unadmitted common record refuses before reset/profile effects");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); f.existingProfile = nullptr; const auto before = f.action->CapturedStateForAnalysis();
            Check(f.action->ExitForAnalysis() == ExitResult::MissingExistingProfile && f.profileReads == 1 && Same(before, f.action->CapturedStateForAnalysis()),
                "Missing existing profile retains old own target after completed reset");
            Check(f.commands[0]->LastResultForAnalysis() == Command::ResultForAnalysis::Completed && f.commands[0]->GetBytesForAnalysis() != c.command1
                && f.commands[0]->GetBytesForAnalysis()[4] == 0 && f.commands[0]->GetBytesForAnalysis()[0x18] == c.command1[0x18]
                && f.commands[1]->GetBytesForAnalysis() == c.command2 && f.profiles[0] == c.profile1 && f.profiles[1] == c.profile2,
                "Completed real Reset prefix survives analytical missing-profile refusal");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); wxAIAction child; f.BeginCall(); f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Current action graph refuses Exit");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->ExitForAnalysis() == ExitResult::UnqualifiedProfile && !f.actorReads, "Owned action graph refuses Exit");
        }
        {
            const auto c = ExitBase(); ExitFixture f(c); f.BeginCall(); Check(f.action->ExitForAnalysis() == ExitResult::Completed, "Admitted whole Exit completes");
            const auto a = f.commands[0]->GetBytesForAnalysis(), b = f.commands[1]->GetBytesForAnalysis(); const auto profiles = f.profiles; const auto targets = f.targets;
            f.action.reset(); Check(f.commands[0]->GetBytesForAnalysis() == a && f.commands[1]->GetBytesForAnalysis() == b && f.profiles == profiles && f.targets == targets,
                "Borrowed command/profile/target records survive portable action teardown");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay();
        else if (argc == 2 && std::string(argv[1]) == "--exit-replay") ExitReplay();
        else if (argc == 2 && std::string(argv[1]) == "--exit-contracts") { ExitContracts(); std::cout << "Icy bounded captured Exit with common Reset and existing profile contracts passed\n"; }
        else { Contracts(); ExitContracts(); std::cout << "Icy bounded existing-Registry Entry contracts passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "Icy " << error.what() << '\n'; return 1; }
}
