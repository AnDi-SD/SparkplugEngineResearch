#pragma once
// Explicit analytical carrier for original PC renderer offsets, NOT an
// invented original class/ABI or startup identity-matrix assertion.
#include <array>
#include <cstdint>
namespace sparkplug::evidence::pc
{
    struct RendererMatrixStateForAnalysis final
    {
        using RawMatrix=std::array<std::uint32_t,16>;
        std::array<RawMatrix,3> inputs{}; // CA40, CA80, CAC0
        std::array<RawMatrix,3> cached{}; // CB00, CB40, CB80
        bool dirty=false; // F2F4
    };
}
