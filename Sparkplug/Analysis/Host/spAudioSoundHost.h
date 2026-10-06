#pragma once
#include <cstdint>
#include <functional>

namespace sparkplug::host
{
    struct spAudioSoundHost final
    {
        // Borrowed AudioManager group array. The caller provides its actual
        // cached scalar on each of the two ordered native reads. The initial
        // group index selects both reads. Absence is an error, never a zero.
        std::function<bool(std::uint32_t groupIndex, float& value)> readGroupScalar;
    };
}
