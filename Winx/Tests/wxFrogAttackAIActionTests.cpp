#include "Code/wxFrogAttackAIAction.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxFrogAttackAIActionAbi.h"
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
    using Action = wxFrogAttackAIAction;
    using Command = wxAICommandRecordForAnalysis;
    using Profile = Action::ProfileForAnalysis;
    using Result = Action::ResultForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    template<class T> std::string Hex(const T& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    template<std::size_t N> std::array<std::uint8_t, N> Decode(const std::string& encoded)
    {
        Check(encoded.size() == N * 2, "Complete captured byte input"); std::array<std::uint8_t, N> result{};
        for (unsigned i = 0; i != N; ++i) result[i] = static_cast<std::uint8_t>(std::stoul(encoded.substr(i * 2, 2), nullptr, 16));
        return result;
    }
    bool Same(const Action::CapturedState& a, const Action::CapturedState& b)
    { return a.borrowedTarget3A8 == b.borrowedTarget3A8 && a.borrowedCachedCommand3E8 == b.borrowedCachedCommand3E8 && a.rawOwn3AC == b.rawOwn3AC && a.rawOwn3EC == b.rawOwn3EC; }
    struct Config final
    {
        unsigned index = 0, calls = 1, target = 1, cached = 1, changeCached = 0;
        unsigned changeTarget = 0, nextTarget = 0, changePrefix = 0, nextPrefix = 0, changeTail = 0, nextTail = 0;
        std::uint32_t duration = 0x12345678, clearable = 0xDEADBEEF;
        std::array<std::uint8_t, 60> prefix{};
        std::array<std::uint8_t, 4> tail{};
        Command::RawBytesForAnalysis command1{}, command2{};
    };
    struct Fixture final
    {
        std::array<std::array<std::uint8_t, 16>, 2> targets{};
        std::array<std::unique_ptr<Command>, 2> commands;
        std::unique_ptr<Action> action;
        explicit Fixture(const Config& c, Profile profile = Profile::PcCapturedStableCachedCommand,
            Command::ProfileForAnalysis commandProfile = Command::ProfileForAnalysis::PcCaptured703090)
        {
            targets[0].fill(0x4B); targets[1].fill(0x5C);
            commands[0] = std::make_unique<Command>(c.command1, commandProfile); commands[1] = std::make_unique<Command>(c.command2, commandProfile);
            Action::CapturedState state{Target(c.target), c.prefix, Cached(c.cached), c.tail};
            action = std::make_unique<Action>(state, profile);
            action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        ~Fixture() { action.reset(); }
        void* Target(unsigned id) { Check(id <= 2, "Live target ID"); return id ? targets[id - 1].data() : nullptr; }
        Command* Cached(unsigned id) { Check(id <= 2, "Live cached command ID"); return id ? commands[id - 1].get() : nullptr; }
        unsigned TargetID(const void* target) const
        { if (!target) return 0; for (unsigned i = 0; i != 2; ++i) if (target == targets[i].data()) return i + 1; throw std::runtime_error("Live target identity"); }
        unsigned CachedID(const Command* command) const
        { if (!command) return 0; for (unsigned i = 0; i != 2; ++i) if (command == commands[i].get()) return i + 1; throw std::runtime_error("Live cached command identity"); }
        std::string Snapshot() const
        {
            const auto& s = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"targetID\":" << TargetID(s.borrowedTarget3A8) << ",\"cachedCommandID\":" << CachedID(s.borrowedCachedCommand3E8)
                << ",\"rawOwn3AC\":\"" << Hex(s.rawOwn3AC) << "\",\"rawOwn3EC\":\"" << Hex(s.rawOwn3EC)
                << "\",\"command1\":\"" << Hex(commands[0]->GetBytesForAnalysis()) << "\",\"command2\":\"" << Hex(commands[1]->GetBytesForAnalysis())
                << "\",\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis() << '}';
            return out.str();
        }
    };
    Config Base()
    {
        Config c; c.prefix.fill(0xA5); c.tail.fill(0x5A); c.command1.fill(0xFF); c.command2.fill(0x3C);
        for (auto* raw : {&c.command1, &c.command2}) { (*raw)[0] = 0x90; (*raw)[1] = 0x30; (*raw)[2] = 0x70; (*raw)[3] = 0; }
        return c;
    }
    void One(const Config& c, bool emit)
    {
        Check(c.calls >= 1 && c.calls <= 2 && c.cached >= 1 && c.cached <= 2 && c.changeCached <= 2
            && c.changeTarget <= 1 && c.changePrefix <= 1 && c.changeTail <= 1 && c.nextPrefix <= 255 && c.nextTail <= 255, "Bounded replay controls");
        Fixture f(c); std::vector<std::string> steps, changes;
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            if (ordinal == 1 && (c.changeCached || c.changeTarget || c.changePrefix || c.changeTail))
            {
                const auto before = f.Snapshot(); auto& state = f.action->CapturedStateForFixture();
                if (c.changeCached) state.borrowedCachedCommand3E8 = f.Cached(c.changeCached);
                if (c.changeTarget) state.borrowedTarget3A8 = f.Target(c.nextTarget);
                if (c.changePrefix) state.rawOwn3AC.fill(static_cast<std::uint8_t>(c.nextPrefix));
                if (c.changeTail) state.rawOwn3EC.fill(static_cast<std::uint8_t>(c.nextTail));
                std::ostringstream out; out << "{\"call\":1,\"before\":" << before << ",\"after\":" << f.Snapshot() << '}'; changes.push_back(out.str());
            }
            const auto before = f.Snapshot(); const auto own = f.action->CapturedStateForAnalysis();
            const auto old1 = f.commands[0]->GetBytesForAnalysis(), old2 = f.commands[1]->GetBytesForAnalysis();
            auto expected1 = old1, expected2 = old2; auto& selected = own.borrowedCachedCommand3E8 == f.commands[0].get() ? expected1 : expected2;
            selected[0x1D] = 0; selected[0x20] = 0;
            f.action->vfunc_34_ClearForAnalysis(); const auto& after = f.action->CapturedStateForAnalysis();
            Check(f.action->LastResultForAnalysis() == Result::Completed && after.borrowedTarget3A8 == nullptr
                && after.borrowedCachedCommand3E8 == own.borrowedCachedCommand3E8 && after.rawOwn3AC == own.rawOwn3AC && after.rawOwn3EC == own.rawOwn3EC
                && f.commands[0]->GetBytesForAnalysis() == expected1 && f.commands[1]->GetBytesForAnalysis() == expected2
                && f.action->GetDurationMillisecondsForAnalysis() == c.duration && f.action->GetClearableWordForAnalysis() == c.clearable, "Exactly two command bytes and target, preserve all other represented storage");
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
        Config c; std::string prefix, tail, first, second;
        while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.target >> c.cached >> prefix >> tail >> first >> second
                >> c.changeCached >> c.changeTarget >> c.nextTarget >> c.changePrefix >> c.nextPrefix >> c.changeTail >> c.nextTail), "Complete replay row");
            c.prefix = Decode<60>(prefix); c.tail = Decode<4>(tail); c.command1 = Decode<116>(first); c.command2 = Decode<116>(second); One(c, true);
        }
        Check(std::cin.eof(), "Complete input boundary");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<Action> && !std::is_move_constructible_v<Action>);
        { auto c = Base(); c.calls = 2; One(c, false); c.changeCached = 2; c.changeTarget = 1; c.nextTarget = 2; One(c, false); }
        {
            Fixture f(Base()); const auto state = f.action->CapturedStateForAnalysis(); Action omittedProfile(state);
            Check(omittedProfile.ExitForAnalysis() == Result::UnqualifiedProfile && Same(state, omittedProfile.CapturedStateForAnalysis()), "Omitted Exit profile refuses");
            Fixture unqualified(Base(), Profile::Unqualified); const auto before = unqualified.Snapshot();
            Check(unqualified.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == unqualified.Snapshot(), "Unqualified before effects");
        }
        {
            Fixture f(Base()); f.action->CapturedStateForFixture().borrowedCachedCommand3E8 = nullptr; const auto before = f.Snapshot();
            Check(f.action->ExitForAnalysis() == Result::MissingCachedCommand && before == f.Snapshot(), "Missing cached data refuses before effects");
        }
        for (bool wrongTag : {false, true})
        {
            Fixture f(Base(), Profile::PcCapturedStableCachedCommand, wrongTag ? Command::ProfileForAnalysis::PcCaptured703090 : Command::ProfileForAnalysis::Unqualified);
            if (wrongTag) f.commands[0]->BytesForFixture()[0] = 0x91; const auto before = f.Snapshot();
            Check(f.action->ExitForAnalysis() == Result::CommandNotAdmitted && before == f.Snapshot(), "Default/tag command admission refuses without native fault");
        }
        {
            Fixture f(Base()); wxAIAction child; const auto before = f.Snapshot(); f.action->SetCurrentActionForAnalysis(&child);
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == f.Snapshot(), "Current action graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->ExitForAnalysis() == Result::UnqualifiedProfile && before == f.Snapshot(), "Owned action graph refuses");
        }
        {
            Fixture f(Base()); const auto before = f.Snapshot(); f.action->vfunc_0C(nullptr); Check(f.action->LastResultForAnalysis() == Result::UnqualifiedNotify, "Notify refuses");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Unknown Boolean hooks refuse");
            f.action->vfunc_30(); f.action->vfunc_38(); spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refuses");
            bool refused = false; try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(Action::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused && before == f.Snapshot(), "Generic factory requires explicit captured input");
            Check(f.action->vfunc_18().classID == Action::ClassID && f.action->vfunc_18().baseClassID == wxAIAction::ClassID, "Actual typed registration");
        }
        {
            Fixture f(Base()); int opaqueOwner = 1; f.action->SetOwnerForAnalysis(&opaqueOwner); auto* cached = f.action->CapturedStateForAnalysis().borrowedCachedCommand3E8;
            Check(f.action->ExitForAnalysis() == Result::Completed && f.action->GetOwnerForAnalysis() == &opaqueOwner, "Exit does not resolve unrelated owner chain");
            const auto command = cached->GetBytesForAnalysis(); f.action.reset(); Check(cached->GetBytesForAnalysis() == command && f.targets[0][0] == 0x4B && f.targets[1][0] == 0x5C, "Borrowed record/targets survive portable teardown");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(); return 0; }
        Check(argc == 1, "Known check mode"); Contracts(); std::cout << "Frog captured cached-command Exit contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
