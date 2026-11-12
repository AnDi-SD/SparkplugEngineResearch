#include "Code/wxIceWormHolesAIAction.h"
#include "Analysis/PC/wxIceWormHolesAIActionAbi.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace winx::reconstruction;
namespace
{
    using Action = wxIceWormHolesAIAction;
    using Profile = Action::ProfileForAnalysis;
    using Result = Action::ResultForAnalysis;
    using Access = Action::AccessForAnalysis;
    void Check(const bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    template<class Bytes> void Put(Bytes& bytes, const std::size_t offset, const std::uint32_t value)
    { for (unsigned i = 0; i != 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8)); }
    template<class Bytes> std::uint32_t Word(const Bytes& bytes, const std::size_t offset)
    { std::uint32_t value = 0; for (unsigned i = 0; i != 4; ++i) value |= std::uint32_t{bytes[offset + i]} << (i * 8); return value; }
    template<class Bytes> std::string Hex(const Bytes& bytes)
    { std::ostringstream out; out << std::hex << std::setfill('0'); for (const auto b : bytes) out << std::setw(2) << unsigned(b); return out.str(); }
    bool HexDigits(const std::string& text)
    { return std::all_of(text.begin(), text.end(), [](const unsigned char c) { return std::isxdigit(c) != 0; }); }
    std::vector<std::uint8_t> ParseHex(const std::string& text)
    {
        Check(text.size() % 2 == 0 && HexDigits(text), "raw hex digits/width"); std::vector<std::uint8_t> result;
        for (std::size_t i = 0; i < text.size(); i += 2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
        return result;
    }
    std::uint32_t ParseWord(const std::string& text)
    { Check(text.size() == 8 && HexDigits(text), "word digits/width"); return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16)); }
    struct GuardedAction final
    {
        std::array<std::uint8_t, 16> before;
        Action value;
        std::array<std::uint8_t, 16> after;
        GuardedAction(const Action::CapturedStateForAnalysis& state, const Profile profile) : value(state, profile)
        { before.fill(0xD7); after.fill(0xD7); }
        void Canaries() const
        { Check(std::all_of(before.begin(), before.end(), [](auto b) { return b == 0xD7; }) && std::all_of(after.begin(), after.end(), [](auto b) { return b == 0xD7; }), "portable object canaries"); }
    };
    Action::CapturedStateForAnalysis Pattern()
    {
        Action::CapturedStateForAnalysis state{};
        for (std::size_t i = 0; i != state.size(); ++i) state[i] = static_cast<std::uint8_t>(0xA5 + 37 * i);
        Put(state, 0, winx::evidence::pc::wxIceWormHolesAIActionPrimary); Put(state, 0x3A8, 0xDEADBEEF); state[0x3C4] = 0x7F;
        return state;
    }
    Action::RawMessageForAnalysis Message(const std::uint32_t code, const std::uint32_t payload)
    {
        Action::RawMessageForAnalysis message{};
        for (std::size_t i = 0; i != message.size(); ++i) message[i] = static_cast<std::uint8_t>(0x59 + 37 * i);
        Put(message, 0, code); Put(message, 24, payload); return message;
    }
    std::string Trace(const Action& action)
    {
        std::ostringstream out; const auto& trace = action.LastAccessesForAnalysis();
        for (std::size_t i = 0; i != action.LastAccessCountForAnalysis(); ++i)
        {
            const auto& v = trace[i]; if (i) out << ' ';
            out << (v.kind == Access::Kind::Read ? 'R' : 'W') << ':' << (v.role == Access::Role::MessageValue ? 'M' : 'S') << ':'
                << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << v.offset << ':' << std::dec << unsigned(v.bytes) << ':' << std::hex << std::setw(8) << v.value;
        }
        return out.str();
    }
    void Units()
    {
        static_assert(!std::is_polymorphic_v<Action>);
        static_assert(!std::is_copy_constructible_v<Action> && !std::is_move_constructible_v<Action>);
        static_assert(sizeof(Action::RawMessageForAnalysis) == 28);
        static_assert(Action::ClassID == 0x812B81AB && Action::RegistrationBaseClassID == 0x490A6EB5);
        for (const auto code : std::array<std::uint32_t, 9>{0x281C, 0x281D, 0, 0x2738, 0x273A, 0x0001281C, 0xFFFF281D, 0x80002739, 0xFFFFFFFF})
        {
            const auto original = Pattern(); auto expected = original; const auto message = Message(code, 0xFFFFFFFF); const auto saved = message;
            GuardedAction action(original, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
            if (code == 0x281C) expected[0x3C4] = 1;
            if (code == 0x281D) expected[0x3C4] = 0;
            Check(action.value.HandleMessageForAnalysis(message) == Result::Completed, "fullDWORD code admits independent payload");
            Check(action.value.GetCapturedStateForAnalysis() == expected && message == saved, "all968 raw bytes/padding and readonly message preserved"); action.Canaries();
            Check(action.value.LastAccessCountForAnalysis() == ((code == 0x281C || code == 0x281D) ? 2U : 1U), "only selected data accesses");
            const auto& trace = action.value.LastAccessesForAnalysis();
            Check(trace[0].kind == Access::Kind::Read && trace[0].role == Access::Role::MessageValue && trace[0].offset == 0 && trace[0].bytes == 4 && trace[0].value == code, "fullDWORD message read");
            if (code == 0x281C || code == 0x281D)
                Check(trace[1].kind == Access::Kind::Write && trace[1].role == Access::Role::CapturedState && trace[1].offset == 0x3C4 && trace[1].bytes == 1 && trace[1].value == (code == 0x281C ? 1U : 0U), "byte3C4 width and literal");
        }
        for (const auto payload : std::array<std::uint32_t, 4>{0, 0x00000100, 0x80000000, 0xFFFFFF00})
        {
            const auto original = Pattern(); auto expected = original; Put(expected, 0x3A8, 2); const auto message = Message(0x2739, payload);
            GuardedAction action(original, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
            Check(action.value.HandleMessageForAnalysis(message) == Result::Completed && action.value.GetCapturedStateForAnalysis() == expected, "AL-only zero counterexamples and complete raw preservation");
            const auto& trace = action.value.LastAccessesForAnalysis();
            Check(action.value.LastAccessCountForAnalysis() == 3 && trace[1].kind == Access::Kind::Read && trace[1].role == Access::Role::MessageValue && trace[1].offset == 24 && trace[1].bytes == 4 && trace[1].value == payload
                && trace[2].kind == Access::Kind::Write && trace[2].role == Access::Role::CapturedState && trace[2].offset == 0x3A8 && trace[2].bytes == 4 && trace[2].value == 2, "full payloadDWORD then ownDWORD store"); action.Canaries();
        }
        for (const auto payload : std::array<std::uint32_t, 4>{1, 0x80, 0x101, 0xFFFFFFFF})
        {
            const auto original = Pattern(); const auto message = Message(0x2739, payload); const auto saved = message;
            GuardedAction action(original, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
            Check(action.value.HandleMessageForAnalysis(message) == Result::ForeignNodeBranchNotAdmitted && action.value.GetCapturedStateForAnalysis() == original && message == saved && action.value.LastAccessCountForAnalysis() == 0, "foreign path refused before captured mutation/operation trace"); action.Canaries();
        }
        {
            const auto original = Pattern(); GuardedAction action(original, Profile::Unqualified);
            Check(action.value.HandleMessageForAnalysis(Message(0x281C, 0)) == Result::UnqualifiedProfile && action.value.GetCapturedStateForAnalysis() == original && action.value.LastAccessCountForAnalysis() == 0, "default profile refuses before mutation"); action.Canaries();
        }
        {
            auto original = Pattern(); Put(original, 0, 0); GuardedAction action(original, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
            Check(action.value.HandleMessageForAnalysis(Message(0x281C, 0)) == Result::CapturedPrimaryNotAdmitted && action.value.GetCapturedStateForAnalysis() == original && action.value.LastAccessCountForAnalysis() == 0, "bad primary refuses before mutation"); action.Canaries();
        }
        {
            auto expected = Pattern(); GuardedAction action(expected, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
            Check(action.value.HandleMessageForAnalysis(Message(0x281C, 0)) == Result::Completed, "retained first call"); expected[0x3C4] = 1;
            Check(action.value.HandleMessageForAnalysis(Message(0x281C, 0xFFFFFFFF)) == Result::Completed && action.value.GetCapturedStateForAnalysis() == expected && action.value.LastAccessCountForAnalysis() == 2, "repeated equal byte still performs store");
            Put(action.value.CapturedStateForFixture(), 0x3A8, 0x80000000); action.value.CapturedStateForFixture()[0x3C4] = 0x7F; expected = action.value.GetCapturedStateForAnalysis();
            Check(action.value.HandleMessageForAnalysis(Message(0x80002739, 1)) == Result::Completed && action.value.GetCapturedStateForAnalysis() == expected, "changed fullcode preserves supplied own values");
            Check(action.value.HandleMessageForAnalysis(Message(0x2739, 0x101)) == Result::ForeignNodeBranchNotAdmitted && action.value.GetCapturedStateForAnalysis() == expected && action.value.LastAccessCountForAnalysis() == 0, "retained refusal resets trace without state mutation");
            Check(action.value.HandleMessageForAnalysis(Message(0x2739, 0x100)) == Result::Completed, "re-evaluate changed payload low byte"); Put(expected, 0x3A8, 2);
            Check(action.value.GetCapturedStateForAnalysis() == expected, "retained raw3C4/padding untouched by DWORD path"); action.Canaries();
        }
    }
    std::string Replay(const std::string& line)
    {
        std::istringstream in(line); unsigned calls = 0, mask = 0; std::string receiverHex, messageHex, codeHex, payloadHex, ownHex, flagHex, extra;
        Check(bool(in >> calls >> receiverHex >> messageHex >> mask >> codeHex >> payloadHex >> ownHex >> flagHex) && !(in >> extra), "replay syntax");
        Check((calls == 1 || calls == 2) && mask <= 15 && (calls == 2 || mask == 0), "bounded same-instance controls");
        auto receiver = ParseHex(receiverHex), message = ParseHex(messageHex); Check(receiver.size() == 1000 && message.size() == 60, "guarded native projection extents");
        const auto initialReceiver = receiver, initialMessage = message; Action::CapturedStateForAnalysis state{}; std::copy_n(receiver.begin() + 16, state.size(), state.begin());
        GuardedAction action(state, Profile::PcCapturedNoNodeCallsReadOnlyMessage28);
        const auto nextCode = ParseWord(codeHex), nextPayload = ParseWord(payloadHex), nextOwn = ParseWord(ownHex), nextFlag = ParseWord(flagHex); Check(nextFlag <= 255, "next flag byte");
        std::ostringstream out;
        for (unsigned call = 0; call != calls; ++call)
        {
            if (call == 1)
            {
                if (mask & 1) Put(message, 16, nextCode);
                if (mask & 2) Put(message, 16 + 24, nextPayload);
                if (mask & 4) Put(action.value.CapturedStateForFixture(), 0x3A8, nextOwn);
                if (mask & 8) action.value.CapturedStateForFixture()[0x3C4] = static_cast<std::uint8_t>(nextFlag);
            }
            const auto beforeMessage = message; Action::RawMessageForAnalysis input{}; std::copy_n(message.begin() + 16, input.size(), input.begin());
            Check(action.value.HandleMessageForAnalysis(input) == Result::Completed, "admitted replay operation"); action.Canaries();
            std::copy(action.value.GetCapturedStateForAnalysis().begin(), action.value.GetCapturedStateForAnalysis().end(), receiver.begin() + 16);
            Check(message == beforeMessage && std::equal(receiver.begin(), receiver.begin() + 16, initialReceiver.begin()) && std::equal(receiver.end() - 16, receiver.end(), initialReceiver.end() - 16)
                && std::equal(message.begin(), message.begin() + 16, initialMessage.begin()) && std::equal(message.end() - 16, message.end(), initialMessage.end() - 16), "raw receiver/message guards");
            if (call) out << " || "; out << Hex(receiver) << ' ' << Hex(message) << " | " << Trace(action.value);
        }
        return out.str();
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay")
        { std::string line; while (std::getline(std::cin, line)) if (!line.empty()) std::cout << Replay(line) << '\n'; return 0; }
        Check(argc == 1, "unknown arguments"); Units(); std::cout << "IceWormHoles captured NoNodeCalls message contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
