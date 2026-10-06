#include "Code/SparkplugPC/spPCVideoStream.h"
#include "Analysis/PC/spVideoStreamAbi.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
    std::array<unsigned, 9> Scalars(spPCVideoStream& video)
    {
        return {video.NativeHook00ForAnalysis(), video.NativeHook08ForAnalysis(), video.NativeHook0CForAnalysis(),
            video.NativeHook10ForAnalysis(), video.NativeHook14ForAnalysis(), video.NativeHook18ForAnalysis(),
            video.NativeHook1CForAnalysis(), video.NativeHook20ForAnalysis(), video.NativeHook24ForAnalysis()};
    }
    void Run(unsigned seed, bool output)
    {
        unsigned releases = 0;
        auto source = std::make_unique<spPCVideoStream>();
        Check(source->IsExactly(spPCVideoStream::ClassID) && source->IsKindOf(spVideoStream::ClassID)
            && source->IsKindOf(spCrossPlatform::ClassID), "native identity and base chain");
        Check(source->GetStateForAnalysis().byte18 == 0 && source->GetStateForAnalysis().byte19 == 0
            && source->GetStateForAnalysis().word20 == 0 && !source->GetOwnedBufferForAnalysis(), "constructor own defaults");
        const spVideoStream::StateForAnalysis changed{std::uint8_t(seed), std::uint8_t(seed >> 8), seed ^ 0x12345678};
        source->SetStateForAnalysis(changed);
        source->SetName("video-native-name");
        auto* raw = new std::uint8_t[16]; raw[0] = std::uint8_t(seed);
        source->AdoptBufferForAnalysis({raw, [&](std::uint8_t* value) { ++releases; delete[] value; }});
        const auto result = Scalars(*source); source->NativeHook04ForAnalysis(); source->NativeHook28ForAnalysis();
        Check(result == std::array<unsigned, 9>{1,1,1,1,1,1,1,0,0}, "actual native dormant interface returns");
        Check(source->GetStateForAnalysis().word20 == changed.word20 && source->GetOwnedBufferForAnalysis() == raw
            && raw[0] == std::uint8_t(seed) && !releases, "interface leaves own payload and buffer untouched");
        auto owner = source->Clone(); auto* clone = dynamic_cast<spPCVideoStream*>(owner.get());
        Check(clone && !clone->GetOwnedBufferForAnalysis() && clone->GetStateForAnalysis().byte18 == 0
            && clone->GetStateForAnalysis().byte19 == 0 && clone->GetStateForAnalysis().word20 == 0, "Named clone resets own payload");
        Check(std::string(clone->GetName()) == "video-native-name", "Named copy transfers inherited name");
        owner.reset(); Check(releases == 0, "clone does not release source-owned buffer");
        source.reset(); Check(releases == 1, "common destructor releases original own buffer once");
        if (output)
        { std::cout << '['; for (unsigned i=0; i<result.size(); ++i) { if (i) std::cout << ','; std::cout << result[i]; } std::cout << "]\n"; }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
        { unsigned seed; while (std::cin >> seed) Run(seed,true); return 0; }
        for (auto seed : {0U, 1U, 0xFFFFFFFFU, 0x12345678U}) Run(seed,false);
        spVideoStream base; Check(!base.Clone() && !spVideoStream::StaticRTTI().factory, "base remains nonallocating null clone");
        std::cout << "PASS " << checks << '/' << checks << ": native video lifetime and dormant PC interface\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
