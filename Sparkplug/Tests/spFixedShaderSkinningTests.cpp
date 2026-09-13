#include "Analysis/PC/spFixedShaderSkinning.h"

#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace sparkplug::evidence::pc;
namespace
{
    unsigned checks = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    bool Near(float a, float b) { return std::fabs(a - b) < 0.00001f; }

    // Bounded machine-readable boundary for external original-shader probes.
    // No original game data is embedded in this executable.
    int Probe()
    {
        unsigned count = 0;
        if (!(std::cin >> count) || count > 4096) return 2;
        std::cout << std::setprecision(9) << '[';
        for (unsigned sample = 0; sample < count; ++sample)
        {
            unsigned weights = 0, bones = 0;
            FixedSkinVertexForAnalysis input;
            std::array<FixedSkinMatrix, 16> palette{};
            std::cin >> weights >> bones;
            if (bones > palette.size()) return 2;
            for (auto& value : input.position) std::cin >> value;
            for (auto& value : input.normal) std::cin >> value;
            for (auto& value : input.weights) std::cin >> value;
            for (auto& value : input.indices) std::cin >> value;
            for (unsigned i = 0; i < bones; ++i)
                for (auto& row : palette[i]) for (auto& value : row) std::cin >> value;
            if (!std::cin) return 2;
            FixedSkinDeformationForAnalysis result;
            const bool accepted = DeformFixedSkinVertexForAnalysis(input, weights,
                palette.data(), bones, result);
            if (sample) std::cout << ',';
            std::cout << "{\"accepted\":" << (accepted ? "true" : "false");
            if (accepted)
            {
                std::cout << ",\"position\":[";
                for (unsigned i = 0; i < 4; ++i) std::cout << (i ? "," : "") << result.position[i];
                std::cout << "],\"normal\":[";
                for (unsigned i = 0; i < 4; ++i) std::cout << (i ? "," : "") << result.normal[i];
                std::cout << ']';
            }
            std::cout << '}';
        }
        std::cout << "]\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--probe") return Probe();
    try
    {
        const FixedSkinMatrix identity{{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}}};
        auto translated = identity;
        translated[0][3] = 4;
        auto scaled = identity;
        scaled[0][0] = 2; scaled[1][1] = 3;
        std::array<FixedSkinMatrix, 3> palette{identity, translated, scaled};
        FixedSkinVertexForAnalysis input{{2,3,4,1}, {0,0,2,0}, {2,0,0,0}, {0,1,2,0}};
        FixedSkinDeformationForAnalysis result;
        Check(DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "B1 accepted");
        Check(result.position == FixedSkinVector{4,6,8,1}, "B1 uses authored weight2 and preserves homogeneous w");
        Check(result.normal == FixedSkinVector{0,0,1,0}, "weighted normal is normalized");

        input.weights = {.25f,.25f,0,0};
        Check(DeformFixedSkinVertexForAnalysis(input, 2, palette.data(), 3, result), "nonunit B2 accepted");
        Check(result.position == FixedSkinVector{2,1.5f,2,1}, "nonunit last weight is not replaced by remainder");
        input.weights = {1.25f,-.25f,0,0};
        Check(DeformFixedSkinVertexForAnalysis(input, 2, palette.data(), 3, result), "negative B2 accepted");
        Check(result.position == FixedSkinVector{1,3,4,1}, "negative translation contribution is retained");

        input.indices = {2,0,0,0}; input.weights = {1,0,0,0}; input.normal = {1,1,0,1};
        Check(DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "scaled bone accepted");
        Check(Near(result.normal[1] / result.normal[0], 1.5f), "normal uses ordinary linear matrix, not inverse transpose");
        Check(Near(result.normal[3], 1.f / std::sqrt(14.f)), "normal w survives xyz assignment and participates in normalize4");

        input.position[3] = 2; input.indices[0] = 1; input.weights[0] = .5f;
        Check(DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "homogeneous input accepted");
        Check(result.position == FixedSkinVector{5,1.5f,2,2}, "translation multiplies input w; output w is not weighted");

        const auto saved = result;
        Check(!DeformFixedSkinVertexForAnalysis(input, 0, palette.data(), 3, result), "unweighted path not fabricated");
        Check(!DeformFixedSkinVertexForAnalysis(input, 5, palette.data(), 3, result), "unsupported weight count");
        Check(!DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 17, result), "original Fixed array limit");
        Check(!DeformFixedSkinVertexForAnalysis(input, 1, nullptr, 3, result), "missing palette");
        input.indices[0] = 3;
        Check(!DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "active index must be in bounds");
        input.indices[0] = 0; input.weights[0] = std::numeric_limits<float>::quiet_NaN();
        Check(!DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "nonfinite weight refused by host");
        input.weights[0] = 1; input.normal = {};
        Check(!DeformFixedSkinVertexForAnalysis(input, 1, palette.data(), 3, result), "undefined zero normalize not invented");
        Check(result.position == saved.position && result.normal == saved.normal, "host refusals do not publish partial deformation");
        std::cout << "PASS Fixed shader skinning: " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
