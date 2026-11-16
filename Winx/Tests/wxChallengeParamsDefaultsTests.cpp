#include "Code/wxChallengeParams.h"
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

using Params = winx::reconstruction::wxChallengeParams;
using Prefix = Params::CapturedPrefixForAnalysis;
using Profile = Params::DefaultsProfileForAnalysis;
using Result = Params::DefaultsResultForAnalysis;

namespace
{
    unsigned contracts = 0;
    void Require(bool value, const char* message)
    { ++contracts; if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    int Digit(char value)
    { return value >= '0' && value <= '9' ? value - '0' : value >= 'a' && value <= 'f' ? value - 'a' + 10 : value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1; }
    bool Decode(const std::string& text, Prefix& result)
    {
        if (text.size() != 736) return false;
        for (std::size_t i = 0; i != result.size(); ++i)
        { const auto a = Digit(text[i * 2]), b = Digit(text[i * 2 + 1]); if (a < 0 || b < 0) return false; result[i] = std::uint8_t(a * 16 + b); }
        return true;
    }
    std::string Hex(const Prefix& value)
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string text; for (auto byte : value) { text += digits[byte >> 4]; text += digits[byte & 15]; } return text;
    }
    Prefix Reset(const Prefix& before)
    {
        Prefix after; after.fill(0xC7);
        Require(Params::ResetParameterDefaultsForAnalysis(before, after, Profile::PcCapturedPrimaryInterior124) == Result::Completed, "interior defaults complete");
        Require(Hex(after).substr(0x124 * 2) == "c0d40100983a00001000000000000000140000000a000000280000004b0000000a00000014000000280000003c0000006400000096000000c8000000fa0000002c0100005e0100000a000000", "exact original19DWORD default byte image");
        for (std::size_t i = 0; i != 0x124; ++i) Require(after[i] == before[i], "all292 opaque parent bytes preserved");
        auto alias = before;
        Require(Params::ResetParameterDefaultsForAnalysis(alias, alias, Profile::PcCapturedPrimaryInterior124) == Result::Completed && alias == after, "same-buffer staged output");
        Require(Params::ResetParameterDefaultsForAnalysis(alias, alias, Profile::PcCapturedPrimaryInterior124) == Result::Completed && alias == after, "represented defaults idempotent");
        return after;
    }
    Prefix Receiver(unsigned fill, bool ramp)
    {
        Prefix before;
        for (std::size_t i = 0; i != before.size(); ++i) before[i] = std::uint8_t(fill + (ramp ? i * 37 : 0));
        before[0] = 0x90; before[1] = 0x12; before[2] = 0x70; before[3] = 0; return before;
    }
    void Units()
    {
        for (unsigned fill : {0U, 255U, 204U, 165U, 90U, 127U, 128U, 1U})
            for (bool ramp : {false, true}) (void)Reset(Receiver(fill, ramp));
        const auto before = Receiver(0xA5, false);
        Prefix output; output.fill(0xC7); const auto sentinel = output;
        Require(Params::ResetParameterDefaultsForAnalysis(before, output) == Result::UnqualifiedProfile && output == sentinel, "default refusal preserves output");
        auto foreign = before; foreign[0] = 0;
        Require(Params::ResetParameterDefaultsForAnalysis(foreign, output, Profile::PcCapturedPrimaryInterior124) == Result::CapturedPrimaryNotAdmitted && output == sentinel, "foreign primary refusal preserves output");
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream stream(line); std::string raw, extra; Prefix before;
            Require(bool(stream >> raw) && !(stream >> extra) && Decode(raw, before), "exact captured prefix replay row");
            std::cout << Hex(Reset(before)) << '\n';
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) return EXIT_FAILURE;
    Units(); std::cout << "ChallengeParamsDefaults: " << contracts << " contracts passed\n";
    return EXIT_SUCCESS;
}
