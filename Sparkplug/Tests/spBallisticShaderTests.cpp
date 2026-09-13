#include "Analysis/PC/spBallisticShader.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace sparkplug::evidence::pc;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value) throw std::runtime_error(label);
    }
    bool Near(float a, float b) { return std::fabs(a - b) < 0.00001f; }
    int Probe()
    {
        unsigned count = 0;
        if (!(std::cin >> count) || count > 4096) return 2;
        std::cout << std::setprecision(9) << '[';
        const auto read = [](auto& values) { for (auto& value : values) std::cin >> value; };
        const auto write = [](const auto& values)
        {
            std::cout << '[';
            for (unsigned i = 0; i < values.size(); ++i) std::cout << (i ? "," : "") << values[i];
            std::cout << ']';
        };
        for (unsigned sample = 0; sample < count; ++sample)
        {
            BallisticShaderInputForAnalysis input;
            BallisticShaderParametersForAnalysis parameters;
            read(input.position); read(input.velocity);
            std::cin >> input.birthTime >> input.lifetime;
            for (auto& row : parameters.viewProjection) read(row);
            read(parameters.accelerationBegin); read(parameters.accelerationEnd);
            read(parameters.cameraPosition); read(parameters.timeLoopScales);
            std::cin >> parameters.framebufferWidth;
            read(parameters.colorBegin); read(parameters.colorEnd);
            if (!std::cin) return 2;
            BallisticShaderOutputForAnalysis result;
            const bool accepted = EvaluateBallisticShaderForAnalysis(input, parameters, result);
            if (sample) std::cout << ',';
            std::cout << "{\"accepted\":" << (accepted ? "true" : "false");
            if (accepted)
            {
                std::cout << ",\"age\":" << result.age << ",\"normalizedAge\":" << result.normalizedAge;
                std::cout << ",\"pointSize\":" << result.pointSize << ",\"position\":"; write(result.position);
                std::cout << ",\"clipPosition\":"; write(result.clipPosition);
                std::cout << ",\"color\":"; write(result.color);
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
        BallisticShaderInputForAnalysis input{{0,0,4,1}, {1,0,0,0}, 0, 2};
        BallisticShaderParametersForAnalysis parameters;
        parameters.viewProjection = {{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1}}};
        parameters.cameraPosition = {0,0,0,1};
        parameters.timeLoopScales = {2,0,2,4};
        parameters.framebufferWidth = 8;
        parameters.colorBegin = {.2f,.4f,.6f,.8f};
        parameters.colorEnd = {.8f,.6f,.4f,.2f};
        BallisticShaderOutputForAnalysis result;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result), "end accepted");
        Check(result.age == 2 && result.pointSize == 8, "lifetime endpoint is included");
        Check(result.position[0] == 2 && result.clipPosition[3] == 1, "linear displacement and homogeneous 1");
        Check(result.color == parameters.colorEnd, "end color");

        parameters.timeLoopScales[1] = std::nextafter(.5f, 0.f);
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result) && result.age == 2, "loop below .5 is disabled");
        parameters.timeLoopScales[1] = .5f;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result) && result.age == 0, "loop .5 subtracts at lifetime");
        Check(result.pointSize == 4 && result.color == parameters.colorBegin, "wrapped beginning included");
        parameters.timeLoopScales[0] = 6;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result) && result.age == 4, "loop subtracts exactly once");
        Check(result.pointSize == 0 && result.color[0] > 1, "size masked but color extrapolates");
        parameters.timeLoopScales[0] = -1;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result), "negative age accepted");
        Check(result.position[0] == -1 && result.pointSize == 0 && result.color[0] < 0, "before birth has motion and color expressions");

        parameters.timeLoopScales = {0,0,2,4};
        input.position[3] = 4;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result), "position w accepted");
        Check(Near(result.pointSize, 16.f / 5.f) && result.clipPosition[3] == 1, "dp4 distance uses w, clip translation does not");
        input.position[3] = 1; parameters.timeLoopScales[0] = 2;
        parameters.accelerationBegin[0] = 3; parameters.accelerationEnd[0] = 1;
        Check(EvaluateBallisticShaderForAnalysis(input, parameters, result), "accelerated path accepted");
        Check(Near(result.position[0], 8 + 8 * .166667f), "cubic term retains sign and decimal constant");
        Check(result.pointSize == 8, "point distance uses original position before motion");

        const auto saved = result;
        input.lifetime = 0;
        Check(!EvaluateBallisticShaderForAnalysis(input, parameters, result), "zero lifetime outside host boundary");
        input.lifetime = -1;
        Check(!EvaluateBallisticShaderForAnalysis(input, parameters, result), "negative lifetime outside qualified domain");
        input.lifetime = 2; parameters.cameraPosition = input.position;
        Check(!EvaluateBallisticShaderForAnalysis(input, parameters, result), "zero distance outside host boundary");
        parameters.cameraPosition[0] = std::numeric_limits<float>::quiet_NaN();
        Check(!EvaluateBallisticShaderForAnalysis(input, parameters, result), "nonfinite input refused");
        Check(result.position == saved.position && result.clipPosition == saved.clipPosition
            && result.color == saved.color && result.pointSize == saved.pointSize, "refusals leave output unchanged");
        std::cout << "PASS Ballistic shader: " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
