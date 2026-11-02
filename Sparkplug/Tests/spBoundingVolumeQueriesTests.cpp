#include "Code/Sparkplug/spBoundingVolumeQueries.h"
#include "Analysis/PC/spSphereContactAbi.h"
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
    using Input = spSphereContactInputForAnalysis;
    using Contact = spSphereContactForAnalysis;
    using Profile = spSphereContactProfileForAnalysis;
    using Result = spSphereContactResultForAnalysis;
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    static_assert(sizeof(Input) == 32 && sizeof(Contact) == 28);
    template<std::size_t N> std::string Words(const std::array<std::uint32_t, N>& words)
    {
        std::ostringstream out; out << '[';
        for (unsigned i = 0; i != N; ++i) { if (i) out << ','; out << '"' << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << words[i] << '"'; }
        out << ']'; return out.str();
    }
    std::array<std::uint32_t, 8> InputWords(const Input& value)
    { std::array<std::uint32_t, 8> out; std::memcpy(out.data(), &value, 32); return out; }
    std::array<std::uint32_t, 7> OutputWords(const Contact& value)
    { std::array<std::uint32_t, 7> out; std::memcpy(out.data(), &value, 28); return out; }
    Input DecodeInput(const std::array<std::uint32_t, 8>& words)
    { Input out; std::memcpy(&out, words.data(), 32); return out; }
    Contact Dirty()
    { Contact out; const std::array<std::uint32_t, 7> words{0x7FC12345,0xFFFFFFFF,0x80000000,0xA5A5A5A5,0x5A5A5A5A,0x01234567,0xFEDCBA98}; std::memcpy(&out, words.data(), 28); return out; }
    struct Guarded final
    {
        std::array<std::uint32_t, 4> prefix{0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7};
        Contact output;
        std::array<std::uint32_t, 4> suffix{0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7,0xD7D7D7D7};
        void CheckCanaries() const
        { for (auto x : prefix) Check(x == 0xD7D7D7D7, "Prefix canary"); for (auto x : suffix) Check(x == 0xD7D7D7D7, "Suffix canary"); }
    };
    void Replay()
    {
        unsigned index, calls, change; std::string token;
        while (std::cin >> index)
        {
            Check(bool(std::cin >> calls), "Call count"); std::array<std::uint32_t, 8> words{}, next{}; std::array<std::uint32_t, 7> output{};
            for (auto& word : words) { Check(bool(std::cin >> token), "Input word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            for (auto& word : output) { Check(bool(std::cin >> token), "Output word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            Check(bool(std::cin >> change) && change <= 1 && calls >= 1 && calls <= 2, "Bounded change controls");
            for (auto& word : next) { Check(bool(std::cin >> token), "Next input word"); word = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)); }
            Guarded guarded; std::memcpy(&guarded.output, output.data(), 28); Input input = DecodeInput(words); std::vector<std::string> steps, changes;
            for (unsigned ordinal = 0; ordinal != calls; ++ordinal)
            {
                if (ordinal == 1 && change)
                {
                    const auto before = InputWords(input); input = DecodeInput(next);
                    std::ostringstream out; out << "{\"call\":1,\"inputBefore\":" << Words(before) << ",\"inputAfter\":" << Words(InputWords(input)) << ",\"output\":" << Words(OutputWords(guarded.output)) << '}'; changes.push_back(out.str());
                }
                const auto before = OutputWords(guarded.output); const auto inputBefore = InputWords(input);
                const auto result = ComputeSphereContactForAnalysis(input, guarded.output, Profile::PcFiniteNearest037F);
                Check(result == Result::NoContact || result == Result::Contact, "Admitted whole helper completion");
                Check(inputBefore == InputWords(input), "Input unchanged"); guarded.CheckCanaries();
                std::ostringstream out; out << "{\"ordinal\":" << ordinal << ",\"input\":" << Words(inputBefore) << ",\"outputBefore\":" << Words(before)
                    << ",\"outputAfter\":" << Words(OutputWords(guarded.output)) << ",\"result\":" << (result == Result::Contact ? 1 : 0) << '}'; steps.push_back(out.str());
            }
            std::cout << "{\"steps\":["; for (unsigned i = 0; i != steps.size(); ++i) { if (i) std::cout << ','; std::cout << steps[i]; }
            std::cout << "],\"changes\":["; for (unsigned i = 0; i != changes.size(); ++i) { if (i) std::cout << ','; std::cout << changes[i]; } std::cout << "]}\n";
        }
        Check(std::cin.eof(), "Complete replay input");
    }
    void Contracts()
    {
        Input input{{0,0,0},1,{1,0,0},1}; auto output = Dirty(); const auto before = OutputWords(output);
        Check(ComputeSphereContactForAnalysis(input, output) == Result::UnqualifiedProfile && OutputWords(output) == before, "Default refuses before effects");
        Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::Contact
            && output.position == std::array<float,3>{.5F,0,0} && output.normal == std::array<float,3>{-1,0,0} && output.depth == 1, "Axis contact geometry");
        input.center2[0] = 2; output = Dirty();
        Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::NoContact && OutputWords(output) == before, "Exact tangent omits all output writes");
        input.center2[0] = 3;
        Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::NoContact && OutputWords(output) == before, "Separated output unchanged");
        input = {{-0.0F,0,-0.0F},-1,{0,-0.0F,0},-1};
        Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::Contact && output.depth == -2
            && OutputWords(output)[0] == 0x80000000 && OutputWords(output)[2] == 0x80000000 && output.normal == std::array<float,3>{0,1,0}, "Coincident raw center and signed radius sum preserved");
        input = {{0,0,0},1,{0,0,0},1}; input.center2[0] = 0x1p-80F;
        Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::Contact && output.normal == std::array<float,3>{0,1,0} && output.depth == 2, "F32 squared-distance underflow selects literal zero branch");
        for (std::uint32_t word : {0x7FC12345U,0x7F800000U,0xFF800000U,0x43808000U})
        {
            input = {{0,0,0},1,{0,0,0},1}; std::memcpy(&input.radius1, &word, 4); output = Dirty();
            Check(ComputeSphereContactForAnalysis(input, output, Profile::PcFiniteNearest037F) == Result::OutOfDomain && OutputWords(output) == before, "Nonfinite/outside finite bound refuses analytically");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(); return 0; }
        Check(argc == 1, "Known check mode"); Contracts(); std::cout << "Sphere contact value helper contracts passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
