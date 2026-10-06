#pragma once

// Explicit replacement for the original global renderer. Callbacks provide
// writable storage and device submission; geometry remains in spQuad3D.
#include <array>
#include <cstdint>
#include <functional>

namespace sparkplug::reconstruction { class spBaseObject; }
namespace sparkplug::host
{
    struct spQuad3DVertex final
    {
        std::array<float, 3> position{};
        std::uint32_t color = 0;
        std::array<float, 2> uv{};
    };
    static_assert(sizeof(spQuad3DVertex) == 0x18);

    struct spQuad3DHost final
    {
        // Original requests renderer buffer format 0x900. Returned spans
        // must hold four vertices and six uint16 indices respectively.
        std::function<spQuad3DVertex*(std::uint32_t)> acquireVertices;
        std::function<std::uint16_t*()> acquireIndices;
        std::function<bool()> stateOverride;
        reconstruction::spBaseObject* defaultVertexBuffer = nullptr;
        std::uint32_t worldMatrixToken = 0;
        std::function<void(reconstruction::spBaseObject*, std::uint32_t)> selectVertices;
        std::function<void(reconstruction::spBaseObject*)> applyMaterial;
        std::function<std::uint8_t(std::uint32_t, std::uint32_t, std::uint32_t)> draw;
    };
}
