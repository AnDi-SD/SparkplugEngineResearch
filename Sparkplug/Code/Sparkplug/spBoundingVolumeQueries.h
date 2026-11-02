#pragma once
#include <array>

namespace sparkplug::reconstruction
{
    // Portable value views for the shared PC472270 helper. Its original
    // declaration owner is unknown; these are not native registered classes.
    struct spSphereContactInputForAnalysis final
    {
        std::array<float, 3> center1{};
        float radius1 = 0;
        std::array<float, 3> center2{};
        float radius2 = 0;
    };
    struct spSphereContactForAnalysis final
    {
        std::array<float, 3> position{};
        std::array<float, 3> normal{};
        float depth = 0;
    };
    enum class spSphereContactProfileForAnalysis { Unqualified, PcFiniteNearest037F };
    enum class spSphereContactResultForAnalysis
    { NoContact = 0, Contact = 1, UnqualifiedProfile, OutOfDomain };

    // Our analytical free-function name for the complete native value helper.
    // The admitted profile models nearest/64-significand x87 arithmetic on
    // finite inputs of magnitude <=256, including signed radii and zeros.
    // Input is copied by value. NoContact and admission refusal preserve output.
    spSphereContactResultForAnalysis ComputeSphereContactForAnalysis(
        spSphereContactInputForAnalysis input, spSphereContactForAnalysis& output,
        spSphereContactProfileForAnalysis profile = spSphereContactProfileForAnalysis::Unqualified) noexcept;
}
