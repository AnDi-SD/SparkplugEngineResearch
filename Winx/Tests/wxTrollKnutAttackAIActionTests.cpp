#include "Code/wxTrollAttackAIAction.h"
#include "Code/wxKnutAttackAIAction.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxTrollAttackAIActionAbi.h"
#include "Analysis/PC/wxKnutAttackAIActionAbi.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Command = wxAICommandRecordForAnalysis;
    using Profile = wxAIActionCommandExitProfileForAnalysis;
    using Result = wxAIActionCommandExitResultForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; } return out; }
    std::vector<std::uint8_t> Decode(const std::string& text)
    {
        Check(text.size() % 2 == 0, "Complete hexadecimal input"); std::vector<std::uint8_t> out(text.size() / 2);
        for (std::size_t i = 0; i != out.size(); ++i) out[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
        return out;
    }
    Command::RawBytesForAnalysis CommandBytes(const std::string& text)
    { const auto raw = Decode(text); Check(raw.size() == 116, "Complete116Bcommand"); Command::RawBytesForAnalysis out{}; std::copy(raw.begin(), raw.end(), out.begin()); return out; }
    template<class Action> struct Traits;
    template<> struct Traits<wxTrollAttackAIAction>
    { static constexpr std::size_t Raw = 164; inline static constexpr std::array<unsigned, 6> Flags{{0x1D,0x20,0x21,0x22,0x23,0x24}}; };
    template<> struct Traits<wxKnutAttackAIAction>
    { static constexpr std::size_t Raw = 92; inline static constexpr std::array<unsigned, 7> Flags{{0x1D,0x5C,0x20,0x21,0x22,0x23,0x24}}; };
    struct Config final
    {
        unsigned index = 0, calls = 1, target = 1, owner = 1;
        unsigned ownerActor1 = 1, ownerActor2 = 2, actorCommand1 = 1, actorCommand2 = 2;
        unsigned nextOwner = 0, changeOwnerActor = 0, nextActor = 0, changeActorCommand = 0, nextCommand = 0;
        unsigned changeTarget = 0, nextTarget = 0, changeRaw = 0, nextRaw = 0;
        std::uint32_t duration = 0x12345678, clearable = 0xDEADBEEF;
        std::vector<std::uint8_t> raw;
        Command::RawBytesForAnalysis command1{}, command2{};
    };
    // Our field storage, not native behavior/actor object layouts.
    struct Actor final { Command* command = nullptr; std::uint32_t canary = 0xB1B2B3B4; };
    struct Owner final { Actor* actor = nullptr; std::uint32_t canary = 0xA1A2A3A4; };
    template<class Action> struct Fixture final : wxAIActionCommandExitHost
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<std::unique_ptr<Command>, 2> commands;
        std::array<Actor, 2> actors;
        std::array<Owner, 2> owners;
        std::unique_ptr<Action> action;
        std::string readTrace;
        unsigned actorReads = 0, omitActorRead = 0, omitCommandRead = 0;
        explicit Fixture(const Config& c, Profile profile = Profile::PcCapturedStableCommand,
            Command::ProfileForAnalysis commandProfile = Command::ProfileForAnalysis::PcCaptured703090)
        {
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            commands[0] = std::make_unique<Command>(c.command1, commandProfile); commands[1] = std::make_unique<Command>(c.command2, commandProfile);
            actors[0].command = CommandAt(c.actorCommand1); actors[1].command = CommandAt(c.actorCommand2);
            owners[0].actor = ActorAt(c.ownerActor1); owners[1].actor = ActorAt(c.ownerActor2);
            typename Action::CapturedState state{Target(c.target), {}}; Check(c.raw.size() == state.rawOwn3AC.size(), "Complete opaque own tail");
            std::copy(c.raw.begin(), c.raw.end(), state.rawOwn3AC.begin()); action = std::make_unique<Action>(state, this, profile);
            action->SetOwnerForAnalysis(OwnerAt(c.owner)); action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        ~Fixture() { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Live target ID"); return id ? targets[id - 1].data() : nullptr; }
        Owner* OwnerAt(unsigned id) { Check(id <= 2, "Live owner ID"); return id ? &owners[id - 1] : nullptr; }
        Actor* ActorAt(unsigned id) { Check(id <= 2, "Live actor ID"); return id ? &actors[id - 1] : nullptr; }
        Command* CommandAt(unsigned id) { Check(id <= 2, "Live command ID"); return id ? commands[id - 1].get() : nullptr; }
        unsigned OwnerID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == &owners[i]) return i + 1; throw std::runtime_error("Known owner identity"); }
        unsigned ActorID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == &actors[i]) return i + 1; throw std::runtime_error("Known actor identity"); }
        unsigned CommandID(const Command* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == commands[i].get()) return i + 1; throw std::runtime_error("Known command identity"); }
        unsigned TargetID(const void* value) const
        { if (!value) return 0; for (unsigned i = 0; i != 2; ++i) if (value == targets[i].data()) return i + 1; throw std::runtime_error("Known target identity"); }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        {
            readTrace += 'A'; ++actorReads;
            if (omitActorRead == actorReads) return std::nullopt;
            for (auto& known : owners) if (owner == &known)
            {
                if (actorReads > 1 && known.actor && known.actor->command)
                    for (unsigned i = 0; i + 1 < actorReads; ++i)
                        Check(known.actor->command->GetBytesForAnalysis()[Traits<Action>::Flags[i]] == 0, "Fresh chain follows every completed flag prefix");
                return static_cast<void*>(known.actor);
            }
            return std::nullopt;
        }
        std::optional<Command*> ReadCommand130ForAnalysis(void* actor) noexcept override
        {
            readTrace += 'C'; if (omitCommandRead == actorReads) return std::nullopt;
            for (auto& known : actors) if (actor == &known) return known.command;
            return std::nullopt;
        }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(state.borrowedTarget3A8) << ",\"rawOwn3AC\":\"" << Hex(state.rawOwn3AC)
                << "\",\"command1\":\"" << Hex(commands[0]->GetBytesForAnalysis()) << "\",\"command2\":\"" << Hex(commands[1]->GetBytesForAnalysis())
                << "\",\"ownerID\":" << OwnerID(action->GetOwnerForAnalysis()) << ",\"ownerActorIDs\":[" << ActorID(owners[0].actor) << ',' << ActorID(owners[1].actor)
                << "],\"actorCommandIDs\":[" << CommandID(actors[0].command) << ',' << CommandID(actors[1].command)
                << "],\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
        void CheckCanaries() const
        {
            for (const auto& owner : owners) Check(owner.canary == 0xA1A2A3A4, "Owner preserved");
            for (const auto& actor : actors) Check(actor.canary == 0xB1B2B3B4, "Actor preserved");
            for (auto v : targets[0]) Check(v == 0x4B, "Target1 preserved");
            for (auto v : targets[1]) Check(v == 0x5C, "Target2 preserved");
        }
    };
    template<class Action> Config Base()
    {
        Config c; c.raw.assign(Traits<Action>::Raw, 0xA5); c.command1.fill(0xFF); c.command2.fill(0x3C);
        for (auto* raw : {&c.command1, &c.command2}) { (*raw)[0] = 0x90; (*raw)[1] = 0x30; (*raw)[2] = 0x70; (*raw)[3] = 0; }
        return c;
    }
    template<class Action> void One(const Config& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.owner >= 1 && c.owner <= 2 && c.ownerActor1 >= 1 && c.ownerActor1 <= 2
            && c.ownerActor2 >= 1 && c.ownerActor2 <= 2 && c.actorCommand1 >= 1 && c.actorCommand1 <= 2 && c.actorCommand2 >= 1 && c.actorCommand2 <= 2
            && c.nextOwner <= 2 && c.changeOwnerActor <= 2 && c.nextActor <= 2 && c.changeActorCommand <= 2 && c.nextCommand <= 2
            && c.changeTarget <= 1 && c.nextTarget <= 2 && c.changeRaw <= 1 && c.nextRaw <= 255, "Bounded replay controls");
        Fixture<Action> f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.nextOwner || c.changeOwnerActor || c.changeActorCommand || c.changeTarget || c.changeRaw))
            {
                const auto before = f.Snapshot();
                if (c.nextOwner) f.action->SetOwnerForAnalysis(f.OwnerAt(c.nextOwner));
                if (c.changeOwnerActor) f.OwnerAt(c.changeOwnerActor)->actor = f.ActorAt(c.nextActor);
                if (c.changeActorCommand) f.ActorAt(c.changeActorCommand)->command = f.CommandAt(c.nextCommand);
                if (c.changeTarget) f.action->CapturedStateForFixture().borrowedTarget3A8 = f.Target(c.nextTarget);
                if (c.changeRaw) f.action->CapturedStateForFixture().rawOwn3AC.fill(static_cast<std::uint8_t>(c.nextRaw));
                std::ostringstream out; out << "{\"call\":1,\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); const auto state = f.action->CapturedStateForAnalysis();
            auto expected1 = f.commands[0]->GetBytesForAnalysis(), expected2 = f.commands[1]->GetBytesForAnalysis();
            auto* selected = static_cast<Owner*>(f.action->GetOwnerForAnalysis())->actor->command;
            auto& expected = selected == f.commands[0].get() ? expected1 : expected2; std::string trace;
            for (auto flag : Traits<Action>::Flags) { expected[flag] = 0; trace += "AC"; }
            f.actorReads = 0; f.readTrace.clear(); f.action->vfunc_34_ClearForAnalysis();
            Check(f.action->LastResultForAnalysis() == Result::Completed && f.readTrace == trace && f.actorReads == Traits<Action>::Flags.size(), "Every flag has a fresh chain");
            Check(f.action->CapturedStateForAnalysis().borrowedTarget3A8 == nullptr && f.action->CapturedStateForAnalysis().rawOwn3AC == state.rawOwn3AC
                && f.commands[0]->GetBytesForAnalysis() == expected1 && f.commands[1]->GetBytesForAnalysis() == expected2
                && f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable, "Literal command list then target, other bytes preserved");
            f.CheckCanaries(); std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; steps.push_back(out.str());
        }
        if (emit)
        {
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
    }
    template<class Action> void Replay()
    {
        Config c; std::string raw, first, second;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.target >> c.owner >> c.ownerActor1 >> c.ownerActor2 >> c.actorCommand1 >> c.actorCommand2
                >> raw >> first >> second >> c.nextOwner >> c.changeOwnerActor >> c.nextActor >> c.changeActorCommand >> c.nextCommand >> c.changeTarget >> c.nextTarget >> c.changeRaw >> c.nextRaw), "Complete replay row");
            c.raw = Decode(raw); c.command1 = CommandBytes(first); c.command2 = CommandBytes(second); One<Action>(c, true);
        }
        Check(std::cin.eof(), "Complete input boundary");
    }
    template<class Action> void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<Action> && !std::is_move_constructible_v<Action>);
        { auto c = Base<Action>(); c.calls = 2; One<Action>(c, false); c.nextOwner = 2; One<Action>(c, false); }
        {
            Fixture<Action> f(Base<Action>()); Action omitted(f.action->CapturedStateForAnalysis(), &f);
            Check(omitted.ExitForAnalysis() == Result::UnqualifiedProfile && f.readTrace.empty(), "Omitted profile refuses before reads");
            Fixture<Action> unqualified(Base<Action>(), Profile::Unqualified); const auto before = unqualified.Snapshot();
            Check(unqualified.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == unqualified.Snapshot() && unqualified.readTrace.empty(), "Default profile before effects");
        }
        {
            Fixture<Action> f(Base<Action>()); f.action->SetOwnerForAnalysis(nullptr); const auto before = f.Snapshot();
            Check(f.action->ExitForAnalysis() == Result::MissingOwner && before == f.Snapshot() && f.readTrace.empty(), "Missing owner before access");
            Action missing(f.action->CapturedStateForAnalysis(), nullptr, Profile::PcCapturedStableCommand); missing.SetOwnerForAnalysis(f.OwnerAt(1));
            Check(missing.ExitForAnalysis() == Result::MissingActorStorage && f.commands[0]->GetBytesForAnalysis() == Base<Action>().command1, "Missing host before store");
        }
        for (bool actor : {true, false})
        {
            Fixture<Action> f(Base<Action>()); if (actor) f.owners[0].actor = nullptr; else f.actors[0].command = nullptr; const auto before = f.Snapshot();
            Check(f.action->ExitForAnalysis() == (actor ? Result::MissingActorStorage : Result::MissingCommandStorage) && before == f.Snapshot(), "Missing field storage preserves state");
        }
        for (bool wrongTag : {false, true})
        {
            Fixture<Action> f(Base<Action>(), Profile::PcCapturedStableCommand, wrongTag ? Command::ProfileForAnalysis::PcCaptured703090 : Command::ProfileForAnalysis::Unqualified);
            if (wrongTag) f.commands[0]->BytesForFixture()[0] = 0x91; const auto before = f.Snapshot();
            Check(f.action->ExitForAnalysis() == Result::CommandNotAdmitted && before == f.Snapshot() && f.readTrace == "AC", "Command admission before first store");
        }
        // Analytical negative admission at each chain, not native concurrent
        // mutation, aliasing or fault/rollback qualification.
        for (unsigned stop = 1; stop <= Traits<Action>::Flags.size(); ++stop)
            for (bool actor : {true, false})
            {
                Fixture<Action> f(Base<Action>()); if (actor) f.omitActorRead = stop; else f.omitCommandRead = stop;
                const auto state = f.action->CapturedStateForAnalysis(); auto expected = f.commands[0]->GetBytesForAnalysis();
                for (unsigned i = 0; i + 1 < stop; ++i) expected[Traits<Action>::Flags[i]] = 0;
                Check(f.action->ExitForAnalysis() == (actor ? Result::MissingActorStorage : Result::MissingCommandStorage)
                    && f.commands[0]->GetBytesForAnalysis() == expected && f.action->CapturedStateForAnalysis().borrowedTarget3A8 == state.borrowedTarget3A8
                    && f.action->CapturedStateForAnalysis().rawOwn3AC == state.rawOwn3AC, "Retain every completed prefix before refusal");
            }
        {
            Fixture<Action> f(Base<Action>()); wxAIAction child; const auto before = f.Snapshot(); f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == f.Snapshot() && f.readTrace.empty(), "Current graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == f.Snapshot() && f.readTrace.empty(), "Owned graph refuses");
        }
        {
            Fixture<Action> f(Base<Action>()); const auto before = f.Snapshot(); f.action->vfunc_0C(nullptr); Check(f.action->LastResultForAnalysis() == Result::UnqualifiedNotify, "Notify refuses");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Unknown hooks refuse");
            f.action->vfunc_30(); f.action->vfunc_38(); spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refuses");
            bool refused = false; try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(Action::ClassID); } catch (const std::logic_error&) { refused = true; }
            Check(refused && before == f.Snapshot() && f.readTrace.empty(), "Factory requires explicit captured state");
            Check(f.action->vfunc_18().classID == Action::ClassID && f.action->vfunc_18().baseClassID == wxAIAction::ClassID, "Typed registration");
        }
        {
            Fixture<Action> f(Base<Action>()); Check(f.action->ExitForAnalysis() == Result::Completed, "Admitted Exit"); const auto command = f.commands[0]->GetBytesForAnalysis();
            f.action.reset(); Check(f.commands[0]->GetBytesForAnalysis() == command && f.owners[0].actor == &f.actors[0] && f.actors[0].command == f.commands[0].get(), "Borrowed data survives teardown"); f.CheckCanaries();
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--troll-replay") { Replay<wxTrollAttackAIAction>(); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--knut-replay") { Replay<wxKnutAttackAIAction>(); return 0; }
        Check(argc == 1, "Known paired mode"); Contracts<wxTrollAttackAIAction>(); Contracts<wxKnutAttackAIAction>();
        std::cout << "Troll/Knut captured fresh-chain Exit contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
