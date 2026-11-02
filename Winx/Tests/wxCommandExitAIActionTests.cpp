#include "Code/wxGolemAttackAIAction.h"
#include "Code/wxYetiAttackAIAction.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Code/wxGameTimer.h"
#include "Code/wxCharacterRegistry.h"
#include "Analysis/PC/wxGolemAttackAIActionAbi.h"
#include "Analysis/PC/wxYetiAttackAIActionAbi.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Profile = wxAIActionCommandExitProfileForAnalysis;
    using Result = wxAIActionCommandExitResultForAnalysis;
    using Command = wxAICommandRecordForAnalysis;
    using CommandBytes = Command::RawBytesForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    std::vector<std::uint8_t> Decode(const std::string& encoded, std::size_t size)
    {
        Check(encoded.size() == size * 2, "Complete captured byte input"); std::vector<std::uint8_t> result(size);
        for (unsigned i = 0; i != size; ++i) result[i] = static_cast<std::uint8_t>(std::stoul(encoded.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    CommandBytes DecodeCommand(const std::string& encoded)
    { const auto bytes = Decode(encoded, 116); CommandBytes result{}; std::copy(bytes.begin(), bytes.end(), result.begin()); return result; }
    template<class State> bool Same(const State& a, const State& b)
    { return a.borrowedTarget3A8 == b.borrowedTarget3A8 && a.ownBytes3AC == b.ownBytes3AC; }
    struct Actor { Command* command = nullptr; };
    struct Owner { Actor* actor = nullptr; };
    struct Config
    {
        unsigned index = 0, calls = 1, oldTarget = 1, changeOwner = 0, changeActor = 0, changeCommand = 0;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::vector<std::uint8_t> rest;
        CommandBytes command1{}, command2{};
    };
    template<class Action> struct Fixture final : wxAIActionCommandExitHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<std::unique_ptr<Command>, 2> commands;
        std::array<Actor, 2> actors;
        std::array<Owner, 2> owners;
        std::unique_ptr<Action> action;
        unsigned actorReads = 0, commandReads = 0;
        typename Action::CapturedState before{};
        explicit Fixture(const Config& c, Profile profile = Profile::PcCapturedStableCommand, bool commandAdmitted = true)
        {
            Check(c.rest.size() == Action::RawOwnBytesForAnalysis, "Class-specific complete raw tail"); targets[0].fill(0x4B); targets[1].fill(0x5C);
            const auto commandProfile = commandAdmitted ? Command::ProfileForAnalysis::PcCaptured703090 : Command::ProfileForAnalysis::Unqualified;
            commands[0] = std::make_unique<Command>(c.command1, commandProfile); commands[1] = std::make_unique<Command>(c.command2, commandProfile);
            for (unsigned i = 0; i != 2; ++i) { actors[i].command = commands[i].get(); owners[i].actor = &actors[i]; }
            typename Action::CapturedState state{Target(c.oldTarget), {}}; std::copy(c.rest.begin(), c.rest.end(), state.ownBytes3AC.begin());
            action = std::make_unique<Action>(*this, &owners[0], state, profile);
            action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        ~Fixture() override { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Live target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Live target identity"); }
        unsigned OwnerID(const void* owner) const
        { for (unsigned i = 0; i != 2; ++i) if (owner == &owners[i]) return i + 1; throw std::runtime_error("Live owner identity"); }
        unsigned ActorID(const Actor* actor) const
        { for (unsigned i = 0; i != 2; ++i) if (actor == &actors[i]) return i + 1; throw std::runtime_error("Live actor identity"); }
        unsigned CommandID(const Command* command) const
        { for (unsigned i = 0; i != 2; ++i) if (command == commands[i].get()) return i + 1; throw std::runtime_error("Live command identity"); }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        {
            Check(owner == action->GetOwnerForAnalysis() && !actorReads && !commandReads && Same(before, action->CapturedStateForAnalysis()), "Capture owner/actor before reset or target clear"); ++actorReads;
            for (auto& record : owners) if (owner == &record) return static_cast<void*>(record.actor);
            return std::nullopt;
        }
        std::optional<Command*> ReadCommand130ForAnalysis(void* actor) noexcept override
        {
            Check(actorReads == 1 && !commandReads && Same(before, action->CapturedStateForAnalysis()), "Capture actual common command before reset or own effects"); ++commandReads;
            for (auto& record : actors) if (actor == &record) return record.command;
            return std::nullopt;
        }
        void BeginCall() { actorReads = commandReads = 0; before = action->CapturedStateForAnalysis(); }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"command1\":\"" << Hex(commands[0]->GetBytesForAnalysis()) << "\",\"command2\":\"" << Hex(commands[1]->GetBytesForAnalysis())
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerActorIDs\":[" << ActorID(owners[0].actor) << ',' << ActorID(owners[1].actor)
                << "],\"actorCommandIDs\":[" << CommandID(actors[0].command) << ',' << CommandID(actors[1].command)
                << "],\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    template<class Action> void One(const Config& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.changeOwner <= 2 && c.changeActor <= 2 && c.changeCommand <= 2, "Bounded command Exit replay controls");
        Fixture<Action> f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeOwner || c.changeActor || c.changeCommand))
            {
                const auto before = f.Snapshot();
                if (c.changeOwner) f.action->SetOwnerForAnalysis(&f.owners[c.changeOwner - 1]);
                if (c.changeActor) f.owners[0].actor = &f.actors[c.changeActor - 1];
                if (c.changeCommand) f.actors[0].command = f.commands[c.changeCommand - 1].get();
                std::ostringstream out; out << "{\"call\":1,\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); f.BeginCall(); f.action->vfunc_34_ClearForAnalysis();
            auto* selected = f.owners[f.OwnerID(f.action->GetOwnerForAnalysis()) - 1].actor->command;
            Check(f.action->LastResultForAnalysis() == Result::Completed && f.actorReads == 1 && f.commandReads == 1
                && selected->LastResultForAnalysis() == Command::ResultForAnalysis::Completed, "Class calls real shared record algorithm once");
            Check(f.action->CapturedStateForAnalysis().borrowedTarget3A8 == nullptr && f.action->GetDurationMillisecondsForAnalysis() == c.duration
                && f.action->GetClearableWordForAnalysis() == c.clearable && Hex(f.action->CapturedStateForAnalysis().ownBytes3AC) == Hex(c.rest), "Target clear preserves complete class tail and parent cells");
            std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
    }
    template<class Action> void Replay()
    {
        Config c; std::string rest, a, b;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> a >> b >> c.changeOwner >> c.changeActor >> c.changeCommand), "Complete class-specific replay row");
            c.rest = Decode(rest, Action::RawOwnBytesForAnalysis); c.command1 = DecodeCommand(a); c.command2 = DecodeCommand(b); One<Action>(c, true);
        }
        Check(std::cin.eof(), "Complete class replay input boundary");
    }
    template<class Action> Config Base()
    {
        Config c; c.rest.assign(Action::RawOwnBytesForAnalysis, 0xCC); c.command1.fill(0xA5); c.command2.fill(0x5A);
        for (auto* raw : {&c.command1, &c.command2}) { (*raw)[0] = 0x90; (*raw)[1] = 0x30; (*raw)[2] = 0x70; (*raw)[3] = 0; } return c;
    }
    template<class Action> void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<Action> && !std::is_move_constructible_v<Action>);
        { auto c = Base<Action>(); c.calls = 2; c.changeCommand = 2; One<Action>(c, false); }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c, Profile::Unqualified); f.BeginCall(); const auto state = f.action->CapturedStateForAnalysis();
            Action defaultObject(f, &f.owners[0], state);
            Check(defaultObject.ExitForAnalysis() == Result::UnqualifiedProfile && Same(state, defaultObject.CapturedStateForAnalysis()) && !f.actorReads,
                "Omitted constructor profile defaults to refusal");
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && Same(state, f.action->CapturedStateForAnalysis()) && !f.actorReads, "Default class profile refuses before storage");
        }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c); f.BeginCall(); const auto state = f.action->CapturedStateForAnalysis(); f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->ExitForAnalysis() == Result::MissingOwner && Same(state, f.action->CapturedStateForAnalysis()) && !f.actorReads, "Null owner is analytical refusal");
        }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c); f.BeginCall(); f.owners[0].actor = nullptr;
            Check(f.action->ExitForAnalysis() == Result::MissingActorStorage && !f.commandReads && f.commands[0]->GetBytesForAnalysis() == c.command1, "Missing actor precedes common reset");
        }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c); f.BeginCall(); f.actors[0].command = nullptr;
            Check(f.action->ExitForAnalysis() == Result::MissingCommandStorage && f.commands[0]->GetBytesForAnalysis() == c.command1, "Missing command precedes reset");
        }
        for (bool wrongTag : {false, true})
        {
            const auto c = Base<Action>(); Fixture<Action> f(c, Profile::PcCapturedStableCommand, wrongTag); f.BeginCall(); const auto state = f.action->CapturedStateForAnalysis();
            if (wrongTag) f.commands[0]->BytesForFixture()[0] = 0x91; const auto bytes = f.commands[0]->GetBytesForAnalysis();
            Check(f.action->ExitForAnalysis() == Result::CommandNotAdmitted && Same(state, f.action->CapturedStateForAnalysis())
                && f.commands[0]->GetBytesForAnalysis() == bytes, "Actual shared profile/tag refusal preserves command and own target");
        }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c); wxAIAction child; f.BeginCall(); f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && !f.actorReads, "Current action graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && !f.actorReads, "Owned action graph refuses");
        }
        {
            const auto c = Base<Action>(); Fixture<Action> f(c); const auto before = f.Snapshot(); f.action->vfunc_0C(nullptr);
            Check(f.action->LastResultForAnalysis() == Result::UnqualifiedNotify, "Notify refuses");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Unknown bool hooks refuse");
            f.action->vfunc_30(); f.action->vfunc_38(); spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refuses");
            bool refused = false; try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(Action::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires explicit bindings");
            Check(f.action->IsExactly(Action::ClassID) && f.action->IsKindOf(wxAIAction::ClassID) && f.Snapshot() == before && !f.actorReads, "Class RTTI and unknown hooks preserve state");
            f.BeginCall(); Check(f.action->ExitForAnalysis() == Result::Completed, "Admitted class Exit completes");
            const auto a = f.commands[0]->GetBytesForAnalysis(), b = f.commands[1]->GetBytesForAnalysis(); const auto targets = f.targets;
            f.action.reset(); Check(f.commands[0]->GetBytesForAnalysis() == a && f.commands[1]->GetBytesForAnalysis() == b && f.targets == targets
                && f.commands[0]->ResetForAnalysis() == Command::ResultForAnalysis::Completed, "Borrowed records and targets survive action teardown");
        }
    }
}
namespace Phase54Checks
{
    using Action = wxYetiAttackAIAction;
    using PhaseResult = wxYetiAttackAIActionPhaseResultForAnalysis;
    using PhaseProfile = wxYetiAttackAIActionPhaseProfileForAnalysis;
    template<class Bytes> void Put(Bytes& bytes, std::size_t offset, std::uint32_t value, unsigned size = 4)
    { for (unsigned i = 0; i != size; ++i) bytes[offset - 0x3AC + i] = static_cast<std::uint8_t>(value >> (8 * i)); }
    template<class Bytes> std::uint32_t Word(const Bytes& bytes, std::size_t offset)
    { std::uint32_t value = 0; for (unsigned i = 0; i != 4; ++i) value |= std::uint32_t{bytes[offset - 0x3AC + i]} << (8 * i); return value; }
    struct Input
    {
        Config base;
        std::uint32_t time1 = 0, time2 = 0xFFFFFFFE;
        unsigned changeTimer = 0, changePhase = 0, changeFlag = 0, changeDeadline = 0, changeTime = 0;
        std::uint32_t nextPhase = 0, nextFlag = 0, nextDeadline = 0, nextTime = 0;
    };
    struct PhaseFixture final : wxAIActionCommandExitHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<std::unique_ptr<Command>, 2> commands;
        std::array<Actor, 2> actors;
        std::array<Owner, 2> owners;
        std::array<spTimer, 2> clocks;
        std::array<std::unique_ptr<wxGameTimer>, 2> timers;
        std::unique_ptr<Action> action;
        wxYetiAttackAIActionPhaseBindingsForAnalysis binding;
        Action::CapturedState before{};
        unsigned actorReads = 0, commandReads = 0;
        explicit PhaseFixture(const Input& in, bool commandAdmitted = true)
        {
            const auto& c = in.base; Check(c.rest.size() == 232, "Complete phase raw tail"); targets[0].fill(0x4B); targets[1].fill(0x5C);
            const auto profile = commandAdmitted ? Command::ProfileForAnalysis::PcCaptured703090 : Command::ProfileForAnalysis::Unqualified;
            commands[0] = std::make_unique<Command>(c.command1, profile); commands[1] = std::make_unique<Command>(c.command2, profile);
            for (unsigned i = 0; i != 2; ++i)
            {
                actors[i].command = commands[i].get(); owners[i].actor = &actors[i]; timers[i] = std::make_unique<wxGameTimer>(clocks[i]);
                auto state = timers[i]->GetStateForAnalysis(); state.currentMilliseconds = i ? in.time2 : in.time1; timers[i]->SetStateForAnalysis(state);
            }
            Action::CapturedState state{Target(c.oldTarget), {}}; std::copy(c.rest.begin(), c.rest.end(), state.ownBytes3AC.begin());
            // Exit admission remains default-unqualified; phase is independent.
            action = std::make_unique<Action>(*this, &owners[0], state);
            action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
            binding.profile = PhaseProfile::PcCapturedStablePhase; binding.ownerStorage = this; binding.existingTimer = timers[0].get(); action->BindPhase54ForAnalysis(&binding);
        }
        ~PhaseFixture() override { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Phase target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == targets[i].data()) return i + 1; throw std::runtime_error("Phase target identity"); }
        unsigned OwnerID(const void* value) const
        { for (unsigned i = 0; i != 2; ++i) if (value == &owners[i]) return i + 1; throw std::runtime_error("Phase owner identity"); }
        unsigned ActorID(const Actor* value) const
        { for (unsigned i = 0; i != 2; ++i) if (value == &actors[i]) return i + 1; throw std::runtime_error("Phase actor identity"); }
        unsigned CommandID(const Command* value) const
        { for (unsigned i = 0; i != 2; ++i) if (value == commands[i].get()) return i + 1; throw std::runtime_error("Phase command identity"); }
        unsigned TimerID(const wxGameTimer* value) const
        { for (unsigned i = 0; i != 2; ++i) if (value == timers[i].get()) return i + 1; throw std::runtime_error("Existing typed Timer identity"); }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        {
            Check(owner == action->GetOwnerForAnalysis() && !actorReads && !commandReads, "Fresh captured phase owner resolution");
            const auto& bytes = action->CapturedStateForAnalysis().ownBytes3AC;
            if (Word(before.ownBytes3AC, 0x3B0) == 0) Check(Word(bytes, 0x3BC) == 0, "Phase0 own clear precedes actor read");
            else Check(bytes[0x3C0 - 0x3AC] == 1 && bytes[0x3B8 - 0x3AC] == 0, "Phase1 own flag prefix precedes actor read");
            ++actorReads; for (auto& record : owners) if (owner == &record) return static_cast<void*>(record.actor); return std::nullopt;
        }
        std::optional<Command*> ReadCommand130ForAnalysis(void* actor) noexcept override
        {
            Check(actorReads == 1 && !commandReads, "Phase command follows actor read"); ++commandReads;
            for (auto& record : actors) if (actor == &record) return record.command; return std::nullopt;
        }
        void BeginCall() { before = action->CapturedStateForAnalysis(); actorReads = commandReads = 0; }
        std::string TimerSnapshot(unsigned index) const
        {
            const auto& timer = *timers[index]; const auto& state = timer.GetStateForAnalysis(); std::uint32_t bits; std::memcpy(&bits, &state.deltaSeconds, 4); std::ostringstream out;
            out << "{\"active\":" << state.active << ",\"relative\":" << state.relative << ",\"current1C\":" << state.currentMilliseconds
                << ",\"start20\":" << state.startMilliseconds << ",\"paused24\":" << state.pausedMilliseconds << ",\"delta28Bits\":" << bits << ",\"paused40\":" << unsigned(timer.GetPausedByteForAnalysis()) << '}';
            return out.str();
        }
        std::string ClockSnapshot() const
        {
            std::ostringstream out; for (const auto& clock : clocks) { const auto& s = clock.GetStateForAnalysis(); out << unsigned(s.active) << ':' << s.accumulated << ':' << s.startedAt << ':' << unsigned(s.limited) << ':' << s.limit << ';'; } return out.str();
        }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"command1\":\"" << Hex(commands[0]->GetBytesForAnalysis()) << "\",\"command2\":\"" << Hex(commands[1]->GetBytesForAnalysis())
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerActorIDs\":[" << ActorID(owners[0].actor) << ',' << ActorID(owners[1].actor)
                << "],\"actorCommandIDs\":[" << CommandID(actors[0].command) << ',' << CommandID(actors[1].command) << "],\"timerBindingID\":" << TimerID(binding.existingTimer)
                << ",\"timer1\":" << TimerSnapshot(0) << ",\"timer2\":" << TimerSnapshot(1) << ",\"duration370\":" << action->GetDurationMillisecondsForAnalysis()
                << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}'; return out.str();
        }
    };
    void One(const Input& in, bool emit)
    {
        const auto& c = in.base; Check(c.calls >= 1 && c.calls <= 2 && c.changeOwner <= 2 && c.changeActor <= 2 && c.changeCommand <= 2 && in.changeTimer <= 2, "Bounded whole phase replay");
        PhaseFixture f(in); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeOwner || c.changeActor || c.changeCommand || in.changeTimer || in.changePhase || in.changeFlag || in.changeDeadline || in.changeTime))
            {
                const auto before = f.Snapshot();
                if (c.changeOwner) f.action->SetOwnerForAnalysis(&f.owners[c.changeOwner - 1]);
                if (c.changeActor) f.owners[0].actor = &f.actors[c.changeActor - 1];
                if (c.changeCommand) f.actors[0].command = f.commands[c.changeCommand - 1].get();
                auto& bytes = f.action->CapturedStateForFixture().ownBytes3AC;
                if (in.changePhase) Put(bytes, 0x3B0, in.nextPhase);
                if (in.changeFlag) Put(bytes, 0x3B8, in.nextFlag, 1);
                if (in.changeDeadline) Put(bytes, 0x3E4, in.nextDeadline);
                if (in.changeTime) { auto s = f.timers[0]->GetStateForAnalysis(); s.currentMilliseconds = in.nextTime; f.timers[0]->SetStateForAnalysis(s); }
                if (in.changeTimer) f.binding.existingTimer = f.timers[in.changeTimer - 1].get();
                std::ostringstream out; out << "{\"call\":1,\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(), timer1 = f.TimerSnapshot(0), timer2 = f.TimerSnapshot(1), clocks = f.ClockSnapshot(); f.BeginCall();
            const auto phase = Word(f.before.ownBytes3AC, 0x3B0);
            Check(f.action->Phase54ForAnalysis() == PhaseResult::Completed && f.action->LastPhase54ResultForAnalysis() == PhaseResult::Completed, "Whole admitted Yeti phase completes");
            Check(f.actorReads == unsigned(phase <= 1) && f.commandReads == unsigned(phase <= 1), "Only actual phase branches resolve command");
            Check(f.TimerSnapshot(0) == timer1 && f.TimerSnapshot(1) == timer2 && f.ClockSnapshot() == clocks
                && !f.timers[0]->QueueForAnalysis().PendingCountForAnalysis() && !f.timers[1]->QueueForAnalysis().PendingCountForAnalysis(), "Stored Timer views/global clocks/owned queues remain unchanged");
            Check(f.action->CapturedStateForAnalysis().borrowedTarget3A8 == f.Target(c.oldTarget) && f.action->GetDurationMillisecondsForAnalysis() == c.duration
                && f.action->GetClearableWordForAnalysis() == c.clearable, "Phase preserves target and parent cells");
            std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
    }
    void Replay()
    {
        Input in; std::string rest, a, b; auto& c = in.base;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.oldTarget >> rest >> a >> b >> in.time1 >> in.time2
                >> c.changeOwner >> c.changeActor >> c.changeCommand >> in.changePhase >> in.nextPhase >> in.changeFlag >> in.nextFlag
                >> in.changeDeadline >> in.nextDeadline >> in.changeTime >> in.nextTime >> in.changeTimer), "Complete whole phase replay row");
            c.rest = Decode(rest, 232); c.command1 = DecodeCommand(a); c.command2 = DecodeCommand(b); One(in, true);
        }
        Check(std::cin.eof(), "Complete phase replay input boundary");
    }
    Input Base(std::uint32_t phase, std::uint8_t flag = 0, std::uint32_t deadline = 0, std::uint32_t now = 0xFFFFFFFF)
    {
        Input in; in.base = ::Base<Action>(); in.time1 = now; Put(in.base.rest, 0x3B0, phase); Put(in.base.rest, 0x3B8, flag, 1); Put(in.base.rest, 0x3E4, deadline); return in;
    }
    void Contracts()
    {
        { auto in = Base(2); in.base.calls = 2; One(in, false); }
        { auto in = Base(1, 255); in.base.calls = 2; in.changeTimer = 2; in.changeFlag = 1; in.nextFlag = 128; in.changeDeadline = 1; One(in, false); }
        {
            const auto in = Base(0); PhaseFixture f(in); const auto before = f.Snapshot(); f.BeginCall(); f.action->BindPhase54ForAnalysis(nullptr);
            Check(f.action->Phase54ForAnalysis() == PhaseResult::UnqualifiedProfile && f.Snapshot() == before && !f.actorReads, "Default-unbound phase refuses before effects");
            f.action->BindPhase54ForAnalysis(&f.binding); f.binding.profile = PhaseProfile::Unqualified;
            Check(f.action->Phase54ForAnalysis() == PhaseResult::UnqualifiedProfile && f.Snapshot() == before, "Separate phase profile defaults unqualified");
            f.binding.profile = PhaseProfile::PcCapturedStablePhase;
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && f.Snapshot() == before, "Phase admission does not admit Exit");
            wxAIAction child; f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->Phase54ForAnalysis() == PhaseResult::UnqualifiedProfile && f.Snapshot() == before, "Current graph refuses phase before effects");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->Phase54ForAnalysis() == PhaseResult::UnqualifiedProfile && f.Snapshot() == before, "Owned graph refuses phase before effects");
        }
        {
            const auto in = Base(0); PhaseFixture f(in); f.BeginCall(); f.action->SetOwnerForAnalysis(nullptr); const auto old = f.commands[0]->GetBytesForAnalysis();
            Check(f.action->Phase54ForAnalysis() == PhaseResult::MissingOwner && Word(f.action->CapturedStateForAnalysis().ownBytes3AC, 0x3BC) == 0
                && !f.actorReads && f.commands[0]->GetBytesForAnalysis() == old, "Phase0 missing owner retains own clear before dependent reads");
        }
        {
            const auto in = Base(0); PhaseFixture f(in); f.BeginCall(); f.binding.ownerStorage = nullptr;
            Check(f.action->Phase54ForAnalysis() == PhaseResult::MissingOwnerStorage && Word(f.action->CapturedStateForAnalysis().ownBytes3AC, 0x3BC) == 0, "Missing storage retains phase0 prefix");
        }
        {
            const auto in = Base(0); PhaseFixture f(in); f.BeginCall(); f.owners[0].actor = nullptr;
            Check(f.action->Phase54ForAnalysis() == PhaseResult::MissingActorStorage && !f.commandReads, "Missing actor refuses after own prefix");
        }
        {
            const auto in = Base(1, 255); PhaseFixture f(in); f.BeginCall(); f.binding.existingTimer = nullptr;
            const auto before = f.action->CapturedStateForAnalysis(); const auto cmd = f.commands[0]->GetBytesForAnalysis();
            Check(f.action->Phase54ForAnalysis() == PhaseResult::MissingExistingTimer && !f.actorReads && f.commands[0]->GetBytesForAnalysis() == cmd, "Missing needed Timer precedes command reads");
            auto expected = before; expected.ownBytes3AC[0x3C0 - 0x3AC] = 1;
            Check(Same(expected, f.action->CapturedStateForAnalysis()), "Missing Timer retains exactly3C0 prefix without rollback");
        }
        for (bool flag : {false, true})
        {
            const auto in = Base(1, flag ? 255 : 0, flag ? 1 : 0); PhaseFixture f(in); f.BeginCall(); f.binding.existingTimer = nullptr;
            Check(f.action->Phase54ForAnalysis() == PhaseResult::Completed, "Unneeded Timer storage is not required");
        }
        for (unsigned refusal = 0; refusal != 3; ++refusal)
        {
            const auto in = Base(1, 255); PhaseFixture f(in, refusal != 1); f.BeginCall();
            if (refusal == 0) f.actors[0].command = nullptr;
            if (refusal == 2) f.commands[0]->BytesForFixture()[0] = 0x91;
            const auto old = f.commands[0]->GetBytesForAnalysis(); const auto result = f.action->Phase54ForAnalysis(); const auto& bytes = f.action->CapturedStateForAnalysis().ownBytes3AC;
            Check(result == (refusal == 0 ? PhaseResult::MissingCommandStorage : PhaseResult::CommandNotAdmitted) && Word(bytes, 0x3E4) == 799
                && bytes[0x3B8 - 0x3AC] == 0 && bytes[0x3C0 - 0x3AC] == 1 && f.commands[0]->GetBytesForAnalysis() == old, "Late command refusal retains full Timer and flag prefix");
        }
        {
            const auto in = Base(0x80000000); PhaseFixture f(in); f.BeginCall(); f.action->SetOwnerForAnalysis(nullptr); f.binding.ownerStorage = nullptr; f.binding.existingTimer = nullptr;
            Check(f.action->Phase54ForAnalysis() == PhaseResult::Completed && !f.actorReads && !f.commandReads, "Other raw phases need no owner or Timer");
        }
        {
            const auto in = Base(1, 255); PhaseFixture f(in); f.BeginCall(); Check(f.action->Phase54ForAnalysis() == PhaseResult::Completed, "Borrowed phase completion");
            const auto a = f.commands[0]->GetBytesForAnalysis(), b = f.commands[1]->GetBytesForAnalysis(); const auto targets = f.targets; const auto t1 = f.TimerSnapshot(0), t2 = f.TimerSnapshot(1);
            f.action.reset(); Check(f.commands[0]->GetBytesForAnalysis() == a && f.commands[1]->GetBytesForAnalysis() == b && f.targets == targets
                && f.TimerSnapshot(0) == t1 && f.TimerSnapshot(1) == t2, "Borrowed Timer/command/target storage survives facade destruction");
        }
    }
}
namespace GolemEntryChecks
{
    using Action = wxGolemAttackAIAction;
    using Registry = wxCharacterRegistry;
    using EntryResult = wxGolemAttackAIActionEntryResultForAnalysis;
    using EntryProfile = wxGolemAttackAIActionEntryProfileForAnalysis;
    struct RegistryInput
    {
        unsigned cache = 0;
        bool allocatedOuter = false;
        std::vector<std::vector<unsigned>> groups;
    };
    struct Input
    {
        unsigned index = 0, calls = 1, owner = 1, oldTarget = 0, changeOwner = 0, changeRegistry = 0;
        std::uint32_t duration = 17, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 20> rest{};
        std::array<RegistryInput, 2> registries;
    };
    std::uint32_t Word(const std::array<std::uint8_t, 20>& raw, std::size_t offset)
    {
        std::uint32_t value = 0;
        for (unsigned i = 0; i != 4; ++i) value |= std::uint32_t{raw[offset - 0x3AC + i]} << (8 * i);
        return value;
    }
    struct RegistryStorage
    {
        std::vector<std::vector<void*>> payloads;
        std::vector<Registry::GroupForAnalysis> groups;
        std::vector<const Registry::GroupForAnalysis*> outer;
        std::array<const Registry::GroupForAnalysis*, 1> allocatedEmpty{};
        std::unique_ptr<Registry> value;
        template<class Target>
        RegistryStorage(const RegistryInput& input, Target target,
            Registry::ProfileForAnalysis profile = Registry::ProfileForAnalysis::PcExistingBorrowedVectors)
        {
            Check(input.groups.empty() || input.allocatedOuter, "Borrowed outer storage for groups");
            payloads.resize(input.groups.size()); groups.resize(input.groups.size()); outer.resize(input.groups.size());
            for (std::size_t i = 0; i != input.groups.size(); ++i)
            {
                for (unsigned id : input.groups[i]) payloads[i].push_back(target(id));
                groups[i].borrowedTargets = {payloads[i].empty() ? nullptr : payloads[i].data(), payloads[i].size()};
                outer[i] = &groups[i];
            }
            const auto* storage = input.allocatedOuter ? (outer.empty() ? allocatedEmpty.data() : outer.data()) : nullptr;
            value = std::make_unique<Registry>(Registry::CapturedStateForAnalysis{target(input.cache), {storage, outer.size()}}, profile);
        }
    };
    struct Fixture final : wxAIActionCommandExitHost, wxGolemAttackAIActionEntryHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        struct OwnerStorage { void* entity24 = nullptr; };
        std::array<OwnerStorage, 2> owners;
        std::array<std::unique_ptr<RegistryStorage>, 2> registries;
        wxGolemAttackAIActionEntryBindingsForAnalysis binding;
        std::unique_ptr<Action> action;
        unsigned entityReads = 0;
        bool missingEntityStorage = false;
        explicit Fixture(const Input& in)
        {
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            for (unsigned i = 0; i != 2; ++i)
                registries[i] = std::make_unique<RegistryStorage>(in.registries[i], [this](unsigned id) { return Target(id); });
            action = std::make_unique<Action>(*this, &owners[in.owner - 1], Action::CapturedState{Target(in.oldTarget), in.rest});
            action->SetDurationMillisecondsForAnalysis(in.duration); action->SetClearableWordForAnalysis(in.clearable);
            binding.profile = EntryProfile::PcCapturedPositiveRegistryNoEntity; binding.registry.existing = registries[0]->value.get(); binding.ownerStorage = this;
            action->BindEntryForAnalysis(&binding);
        }
        void* Target(unsigned id) { Check(id <= 2, "Golem borrowed target ID"); return id ? targets[id - 1].data() : nullptr; }
        unsigned TargetID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == targets[i].data()) return i + 1; throw std::runtime_error("Golem target identity"); }
        unsigned OwnerID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == &owners[i]) return i + 1; throw std::runtime_error("Golem captured owner identity"); }
        unsigned RegistryID(const Registry* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == registries[i]->value.get()) return i + 1; throw std::runtime_error("Golem existing Registry identity"); }
        std::optional<void*> ReadActor144ForAnalysis(void*) noexcept override { std::terminate(); }
        std::optional<Command*> ReadCommand130ForAnalysis(void*) noexcept override { std::terminate(); }
        std::optional<void*> ReadEntity24ForAnalysis(void* owner) noexcept override
        {
            Check(owner == action->GetOwnerForAnalysis(), "Entry uses captured owner identity");
            const auto& state = action->CapturedStateForAnalysis(); const auto& raw = state.ownBytes3AC;
            Check(Word(raw, 0x3AC) == 0 && Word(raw, 0x3B4) == 0x481C4000 && raw[0x3BC - 0x3AC] == 0 && Word(raw, 0x3B0) == 0,
                "All five own effects precede dependent owner24 observation");
            Check(state.borrowedTarget3A8 == binding.registry.existing->GetCapturedStateForAnalysis().cachedTarget,
                "Published target comes from the actual shared Registry");
            ++entityReads; if (missingEntityStorage) return std::nullopt;
            for (auto& record : owners) if (owner == &record) return record.entity24;
            return std::nullopt;
        }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"globalRegistry\":" << RegistryID(binding.registry.existing) << ",\"registryCaches\":["
                << TargetID(registries[0]->value->GetCapturedStateForAnalysis().cachedTarget) << ',' << TargetID(registries[1]->value->GetCapturedStateForAnalysis().cachedTarget)
                << "],\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"ownRest3AC\":\"" << Hex(state.ownBytes3AC)
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerEntity24\":[" << TargetID(owners[0].entity24) << ',' << TargetID(owners[1].entity24)
                << "],\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    void One(const Input& in, bool emit)
    {
        Check(in.calls >= 1 && in.calls <= 2 && in.owner >= 1 && in.owner <= 2 && in.changeOwner <= 2 && in.changeRegistry <= 2, "Bounded captured Golem Entry input");
        Fixture f(in); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != in.calls; ++ordinal)
        {
            if (ordinal && (in.changeOwner || in.changeRegistry))
            {
                const auto before = f.Snapshot();
                if (in.changeOwner) f.action->SetOwnerForAnalysis(&f.owners[in.changeOwner - 1]);
                if (in.changeRegistry) f.binding.registry.existing = f.registries[in.changeRegistry - 1]->value.get();
                changes.push_back("{\"call\":1,\"before\":" + before + ",\"after\":" + f.Snapshot() + '}');
            }
            const auto before = f.Snapshot(); const auto firstRead = f.entityReads; const auto owners = f.owners; const auto targets = f.targets;
            f.action->vfunc_30();
            Check(f.action->LastEntryResultForAnalysis() == EntryResult::Completed && f.entityReads == firstRead + 1, "Whole admitted positive Registry/no Entity Entry completes");
            Check(f.action->GetDurationMillisecondsForAnalysis() == in.duration && f.action->GetClearableWordForAnalysis() == in.clearable
                && f.targets == targets && f.owners[0].entity24 == owners[0].entity24 && f.owners[1].entity24 == owners[1].entity24,
                "Entry preserves borrowed owners, targets and parent words");
            steps.push_back("{\"ordinal\":" + std::to_string(ordinal) + ",\"before\":" + before + ",\"after\":" + f.Snapshot() + '}');
        }
        const auto targets = f.targets; const auto owner0 = f.owners[0].entity24, owner1 = f.owners[1].entity24;
        const auto cache0 = f.registries[0]->value->GetCapturedStateForAnalysis().cachedTarget, cache1 = f.registries[1]->value->GetCapturedStateForAnalysis().cachedTarget;
        f.action.reset(); Check(f.targets == targets && f.owners[0].entity24 == owner0 && f.owners[1].entity24 == owner1
            && f.registries[0]->value->GetCapturedStateForAnalysis().cachedTarget == cache0 && f.registries[1]->value->GetCapturedStateForAnalysis().cachedTarget == cache1,
            "Action destruction preserves all borrowed Registry, owner and target data");
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
    }
    Input Base()
    {
        Input in; in.rest.fill(0xA5); in.registries[0] = {0, true, {{2, 1}}}; in.registries[1].cache = 2; return in;
    }
    void Contracts()
    {
        { auto in = Base(); in.calls = 2; One(in, false); }
        { auto in = Base(); in.calls = 2; in.changeOwner = 2; in.changeRegistry = 2; One(in, false); }
        {
            Fixture f(Base()); f.action->BindEntryForAnalysis(nullptr); const auto before = f.Snapshot();
            Check(f.action->EnterForAnalysis() == EntryResult::UnqualifiedProfile && f.Snapshot() == before && !f.entityReads, "Default-unbound Entry refuses before effects");
            f.action->BindEntryForAnalysis(&f.binding); f.binding.profile = EntryProfile::Unqualified;
            Check(f.action->EnterForAnalysis() == EntryResult::UnqualifiedProfile && f.Snapshot() == before, "Separate Entry profile defaults unqualified");
            f.binding.profile = EntryProfile::PcCapturedPositiveRegistryNoEntity;
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && f.Snapshot() == before, "Entry profile does not admit Exit");
            wxAIAction child; f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->EnterForAnalysis() == EntryResult::UnqualifiedProfile && f.Snapshot() == before, "Nonempty inherited current graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis() == EntryResult::UnqualifiedProfile && f.Snapshot() == before, "Owned inherited graph refuses");
        }
        {
            Fixture f(Base()); f.binding.registry.existing = nullptr; const auto before = f.Snapshot();
            Check(f.action->EnterForAnalysis() == EntryResult::MissingExistingRegistry && f.Snapshot() == before && !f.entityReads, "Missing Registry refuses without cold construction");
        }
        {
            Fixture f(Base()); Registry unqualified(Registry::CapturedStateForAnalysis{}); f.binding.registry.existing = &unqualified; const auto before = f.action->CapturedStateForAnalysis();
            Check(f.action->EnterForAnalysis() == EntryResult::RegistryNotAdmitted && Same(before, f.action->CapturedStateForAnalysis()) && !f.entityReads, "Unqualified common Registry refuses before own stores");
        }
        for (unsigned missing = 0; missing != 3; ++missing)
        {
            Fixture f(Base()); if (!missing) f.action->SetOwnerForAnalysis(nullptr); else if (missing == 1) f.binding.ownerStorage = nullptr; else f.missingEntityStorage = true;
            Check(f.action->EnterForAnalysis() == (missing ? EntryResult::MissingOwnerStorage : EntryResult::MissingOwner), "Missing owner data refuses after completed own effects");
            const auto& state = f.action->CapturedStateForAnalysis(); Check(state.borrowedTarget3A8 == f.Target(2) && Word(state.ownBytes3AC, 0x3AC) == 0
                && Word(state.ownBytes3AC, 0x3B0) == 0 && Word(state.ownBytes3AC, 0x3B4) == 0x481C4000 && state.ownBytes3AC[0x3BC - 0x3AC] == 0, "Missing owner/storage retains all five own stores and shared cache prefix");
        }
        for (bool entityPresent : {false, true})
        {
            auto in = Base(); in.registries[0] = {}; Fixture f(in);
            if (entityPresent) f.owners[0].entity24 = f.Target(1);
            Check(f.action->EnterForAnalysis() == (entityPresent ? EntryResult::NotifyNotAdmitted : EntryResult::SelectorNotAdmitted)
                && f.entityReads == 1 && !f.action->CapturedStateForAnalysis().borrowedTarget3A8, "Notify refusal precedes null-target selector refusal without fallback");
        }
        {
            Fixture f(Base()); f.owners[0].entity24 = f.Target(1);
            Check(f.action->EnterForAnalysis() == EntryResult::NotifyNotAdmitted && f.entityReads == 1 && f.action->CapturedStateForAnalysis().borrowedTarget3A8 == f.Target(2), "Positive target does not invent Entity Notify success");
        }
    }
    void Replay()
    {
        Input in; std::string rest;
        while (std::cin >> in.index)
        {
            Check(bool(std::cin >> in.calls >> in.duration >> in.clearable >> in.owner >> in.oldTarget >> rest), "Complete captured Golem Entry prefix");
            const auto bytes = Decode(rest, 20); std::copy(bytes.begin(), bytes.end(), in.rest.begin());
            for (auto& registry : in.registries)
            {
                unsigned allocated = 0, count = 0; Check(bool(std::cin >> registry.cache >> allocated >> count) && allocated <= 1 && count <= 3, "Complete bounded Registry input");
                registry.allocatedOuter = allocated != 0; registry.groups.assign(count, {});
                for (auto& group : registry.groups)
                {
                    unsigned targets = 0; Check(bool(std::cin >> targets) && targets <= 3, "Bounded borrowed target vector"); group.resize(targets);
                    for (auto& id : group) Check(bool(std::cin >> id) && id <= 2, "Borrowed target identity");
                }
            }
            Check(bool(std::cin >> in.changeOwner >> in.changeRegistry), "Complete next-call binding inputs"); One(in, true);
        }
        Check(std::cin.eof(), "Complete Golem Entry replay boundary");
    }
}
int main(int argc, char** argv)
{
    try
    {
        static_assert(sizeof(winx::evidence::pc::wxGolemAttackAIActionLayout) == 960);
        static_assert(sizeof(winx::evidence::pc::wxYetiAttackAIActionLayout) == 1172);
        if (argc == 2 && std::string(argv[1]) == "--golem-replay") Replay<wxGolemAttackAIAction>();
        else if (argc == 2 && std::string(argv[1]) == "--yeti-replay") Replay<wxYetiAttackAIAction>();
        else if (argc == 2 && std::string(argv[1]) == "--phase54-replay") Phase54Checks::Replay();
        else if (argc == 2 && std::string(argv[1]) == "--phase54-contracts") { Phase54Checks::Contracts(); std::cout << "Yeti captured whole54 with existing GameTimer and admitted command23 contracts passed\n"; }
        else if (argc == 2 && std::string(argv[1]) == "--golem-entry-replay") GolemEntryChecks::Replay();
        else if (argc == 2 && std::string(argv[1]) == "--golem-entry-contracts") { GolemEntryChecks::Contracts(); std::cout << "Golem captured whole Entry with positive Registry and null Entity contracts passed\n"; }
        else { Contracts<wxGolemAttackAIAction>(); Contracts<wxYetiAttackAIAction>(); Phase54Checks::Contracts(); GolemEntryChecks::Contracts(); std::cout << "CommandExitAIAction Golem/Yeti captured Exit and real common Reset contracts passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "CommandExitAIAction " << error.what() << '\n'; return 1; }
}
