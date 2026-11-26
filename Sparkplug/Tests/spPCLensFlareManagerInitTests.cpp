#include "Code/SparkplugPC/spPCLensFlareManager.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

using sparkplug::reconstruction::spPCLensFlareManager;

namespace
{
    using State = spPCLensFlareManager::CapturedStateForAnalysis;

    struct Call
    {
        const State* state = nullptr;
        std::uint32_t result = 0;
        std::uint8_t priorCapability = 0;
        unsigned calls = 0;
        bool stateAtProbe = false;
        std::uint8_t enabledAtProbe = 0;
        std::uint8_t capabilityAtProbe = 0;
        std::uint32_t queryType = 0;
    };

    State Fixture(std::uint8_t enabled, std::uint8_t capability)
    {
        return {0x006F2A5C, {0x11223344, 0x11223344, 0x11223344, 0x11223344, 0, 1},
                0, 0x55667788, enabled, capability};
    }

    std::uint32_t SupplyCapability(void* context, std::uint32_t device, std::uint32_t type) noexcept
    {
        auto& call = *static_cast<Call*>(context);
        ++call.calls;
        call.enabledAtProbe = call.state->enabled;
        call.capabilityAtProbe = call.state->capability;
        call.queryType = type;
        call.stateAtProbe = call.state->enabled == 0 && call.state->capability == call.priorCapability
            && call.state->map.first == call.state->map.sentinel
            && call.state->map.root == call.state->map.sentinel
            && call.state->map.last == call.state->map.sentinel && call.state->map.count == 0
            && device == 0x55667788 && type == 9;
        return call.result;
    }
}

int main(int argc, char** argv)
{
    const bool emitTrace = argc == 2 && std::string_view(argv[1]) == "--trace";
    constexpr std::array statuses{0u, 0x8876086Au, 0x80004005u, 0x8876086Bu, 0x7FFFFFFFu, 0xFFFFFFFFu};
    constexpr std::array capabilities{1u, 0u, 1u, 1u, 1u, 1u};
    for (std::size_t index = 0; index < statuses.size(); ++index)
    {
        auto state = Fixture(static_cast<std::uint8_t>(0x87 + index), static_cast<std::uint8_t>(0xA2 + index));
        Call call{&state, statuses[index], state.capability};
        const auto priorEnabled = state.enabled;
        const bool admitted = spPCLensFlareManager::InitExistingEmptyMapForAnalysis(state, SupplyCapability, &call);
        if (!admitted || call.calls != 1 || !call.stateAtProbe || state.enabled != 1
            || state.capability != capabilities[index] || state.outstandingQueries != 0
            || state.map.sentinel != 0x11223344 || state.map.sentinelIsNil != 1
            || state.map.first != 0x11223344 || state.map.root != 0x11223344
            || state.map.last != 0x11223344 || state.map.count != 0 || state.borrowedDevice != 0x55667788)
        {
            std::cerr << "Unexpected initializer projection at status case " << index << '\n';
            return 1;
        }
        if (emitTrace)
        {
            std::cout << statuses[index] << ' ' << unsigned(priorEnabled) << ' '
                      << unsigned(call.priorCapability) << ' ' << unsigned(call.enabledAtProbe) << ' '
                      << unsigned(call.capabilityAtProbe) << ' ' << unsigned(state.enabled) << ' '
                      << unsigned(state.capability) << ' ' << unsigned(admitted) << ' '
                      << call.queryType << '\n';
        }
    }

    auto state = Fixture(0x87, 0x83);
    state.map.count = 1;
    Call call{&state};
    if (spPCLensFlareManager::InitExistingEmptyMapForAnalysis(state, SupplyCapability, &call)
        || call.calls != 0 || state.enabled != 0x87 || state.capability != 0x83 || state.map.count != 1)
    {
        std::cerr << "Nonempty map entered the qualified route\n";
        return 1;
    }
    return 0;
}
