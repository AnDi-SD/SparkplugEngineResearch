#include "Code/Sparkplug/spConstraintValueQueries.h"
#include "Analysis/PC/spConstraintValueQueriesAbi.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Input = spContactTangentsInputForAnalysis;
    using Output = spContactTangentsForAnalysis;
    using Profile = spContactTangentsProfileForAnalysis;
    using Result = spContactTangentsResultForAnalysis;
    static_assert(sizeof(Input) == 12 && sizeof(Output) == 24);
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    std::array<std::uint32_t, 3> InputWords(const Input& value)
    { std::array<std::uint32_t, 3> out; std::memcpy(out.data(), &value, 12); return out; }
    std::array<std::uint32_t, 6> OutputWords(const Output& value)
    { std::array<std::uint32_t, 6> out; std::memcpy(out.data(), &value, 24); return out; }
    Input DecodeInput(const std::array<std::uint32_t, 3>& words)
    { Input out; std::memcpy(&out, words.data(), 12); return out; }
    Output Dirty()
    {
        Output out; const std::array<std::uint32_t, 6> words{
            0x7FC12345,0xFFFFFFFF,0x80000000,0xA5A5A5A5,0x01234567,0xFEDCBA98};
        std::memcpy(&out, words.data(), 24); return out;
    }
    struct Guarded final
    {
        std::array<std::uint32_t, 4> prefix{0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7};
        Output output;
        std::array<std::uint32_t, 4> suffix{0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7};
        void Verify() const
        { for (auto word : prefix) Check(word == 0xD7D7D7D7, "Prefix canary"); for (auto word : suffix) Check(word == 0xD7D7D7D7, "Suffix canary"); }
    };
    template<std::size_t N> std::string Words(const std::array<std::uint32_t, N>& value)
    {
        std::ostringstream out; out << '[';
        for (unsigned i = 0; i != N; ++i)
        { if (i) out << ','; out << '"' << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value[i] << '"'; }
        out << ']'; return out.str();
    }
    void Replay()
    {
        unsigned index, calls, change; std::string token;
        while (std::cin >> index)
        {
            Check(bool(std::cin >> calls), "Call count");
            std::array<std::uint32_t, 3> words{}, next{}; std::array<std::uint32_t, 6> output{};
            for (auto& word : words) { Check(bool(std::cin >> token), "Input word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            for (auto& word : output) { Check(bool(std::cin >> token), "Output word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            Check(bool(std::cin >> change) && change <= 1 && calls >= 1 && calls <= 2, "Bounded change controls");
            for (auto& word : next) { Check(bool(std::cin >> token), "Next input word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            Guarded guarded; std::memcpy(&guarded.output, output.data(), 24);
            Input input = DecodeInput(words); std::vector<std::string> steps, changes;
            for (unsigned ordinal = 0; ordinal != calls; ++ordinal)
            {
                if (ordinal == 1 && change)
                {
                    const auto before = InputWords(input); input = DecodeInput(next);
                    std::ostringstream out; out << "{\"call\":1,\"inputBefore\":" << Words(before)
                        << ",\"inputAfter\":" << Words(InputWords(input)) << ",\"output\":" << Words(OutputWords(guarded.output)) << '}';
                    changes.push_back(out.str());
                }
                const auto before = OutputWords(guarded.output);
                const auto inputBefore = InputWords(input);
                Check(ComputeContactTangentsForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F) == Result::Completed,
                    "Admitted complete tangent helper");
                Check(InputWords(input) == inputBefore, "Input unchanged"); guarded.Verify();
                std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"input\":" << Words(inputBefore)
                    << ",\"outputBefore\":" << Words(before) << ",\"outputAfter\":" << Words(OutputWords(guarded.output)) << '}';
                steps.push_back(out.str());
            }
            std::cout << "{\"steps\":[";
            for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":[";
            for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; }
            std::cout << "]}\n";
        }
        Check(std::cin.eof(), "Complete replay input");
    }
    void Contracts()
    {
        Input input{{0,0,1}}; Guarded guarded; guarded.output = Dirty(); const auto before = OutputWords(guarded.output);
        Check(ComputeContactTangentsForAnalysis(input, guarded.output) == Result::UnqualifiedProfile
            && OutputWords(guarded.output) == before, "Default refuses without output effects");
        Check(ComputeContactTangentsForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F) == Result::Completed
            && OutputWords(guarded.output) == std::array<std::uint32_t,6>{0,0xBF800000,0,0x3F800000,0x80000000,0x80000000},
            "YZ axis and exact signed-zero output words"); guarded.Verify();
        input.direction = {1,0,0};
        Check(ComputeContactTangentsForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F) == Result::Completed
            && OutputWords(guarded.output) == std::array<std::uint32_t,6>{0x80000000,0x3F800000,0,0x80000000,0x80000000,0x3F800000},
            "XY axis and exact signed-zero output words"); guarded.Verify();
        input = DecodeInput({1,1,0});
        Check(ComputeContactTangentsForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F) == Result::Completed
            && OutputWords(guarded.output) == std::array<std::uint32_t,6>{0xBF3504F3,0x3F3504F3,0,0x80000000,0x80000000,1},
            "Extended selected square survives below F32 range"); guarded.Verify();
        const std::array<std::uint32_t,3> refusalInputs[]{
            {0,0,0}, {0,0,0xBF800000}, {0x7FC12345,0,0},
            {0,0x7F800000,0}, {0,0,0xFF800000}, {0x43808000,0,0}};
        for (const auto& inputWords : refusalInputs)
        {
            input = DecodeInput(inputWords); guarded.output = Dirty();
            Check(ComputeContactTangentsForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F) == Result::OutOfDomain
                && OutputWords(guarded.output) == before, "Analytical admission refuses before both outputs"); guarded.Verify();
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(); return 0; }
        Check(argc == 1, "Known check mode"); Contracts();
        std::cout << "Contact tangent value helper contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
