#include "Code/Sparkplug/spSystemSettings.h"
#include "Analysis/PC/spSystemSettingsAbi.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
    void Run(unsigned seed)
    {
        auto source = std::make_unique<spSystemSettings>();
        Check(spSystemSettings::InstanceForAnalysis() == source.get(), "factory publishes singleton primary view");
        Check(source->IsExactly(spSystemSettings::ClassID) && source->IsKindOf(spBaseObject::ClassID), "native identity");
        Check(std::all_of(source->GetOpaqueBytesForAnalysis().begin(), source->GetOpaqueBytesForAnalysis().end(),
            [](auto b) { return b == 0; }) && !source->GetTrailingByteForAnalysis(), "255 zero bytes, final byte unknown");
        spSystemSettings::OpaqueBytesForAnalysis payload{};
        for (unsigned i = 0; i < payload.size(); ++i) payload[i] = std::uint8_t(seed + i * 37);
        source->SetOpaqueBytesForAnalysis(payload, std::uint8_t(seed ^ 0xA5));
        auto owner = source->Clone();
        auto* clone = dynamic_cast<spSystemSettings*>(owner.get());
        Check(clone && spSystemSettings::InstanceForAnalysis() == clone, "clone replaces published singleton");
        Check(std::all_of(clone->GetOpaqueBytesForAnalysis().begin(), clone->GetOpaqueBytesForAnalysis().end(),
            [](auto b) { return b == 0; }) && !clone->GetTrailingByteForAnalysis(), "clone resets payload to factory defaults");
        Check(source->GetOpaqueBytesForAnalysis() == payload && source->GetTrailingByteForAnalysis() == std::uint8_t(seed ^ 0xA5),
            "source payload preserved");
        source.reset();
        Check(spSystemSettings::InstanceForAnalysis() == nullptr, "older object destruction clears newer singleton");
        Check(clone->IsExactly(spSystemSettings::ClassID), "newer clone still alive after singleton reset");
        owner.reset();
        Check(spSystemSettings::InstanceForAnalysis() == nullptr, "final release keeps singleton empty");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
        { unsigned seed; while (std::cin >> seed) { Run(seed); std::cout << "[255,0,1,1,0]\n"; } return 0; }
        for (auto seed : {0u, 1u, 127u, 255u, 0xFFFFFFFFu}) Run(seed);
        std::cout << "PASS " << checks << '/' << checks << ": Settings own lifecycle, clone and singleton policy\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
