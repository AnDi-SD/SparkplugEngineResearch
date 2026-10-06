#include "Code/Sparkplug/spLightController.h"
#include "Code/Sparkplug/spLightData.h"
#include "Code/Sparkplug/spAnimationManager.h"
#include "Analysis/PC/spLightControllerAbi.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(description);
    }
    float Float(std::uint32_t value) { float result; std::memcpy(&result, &value, sizeof(result)); return result; }
    std::uint32_t Bits(float value) { std::uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }

    void RunBatch()
    {
        std::uint32_t borrowed = 0, enabled = 0, first = 0, second = 0, type = 0, clamp = 0;
        while (std::cin >> borrowed >> enabled >> first >> second >> type >> clamp)
        {
            std::array<std::uint32_t, 8> function{};
            std::array<std::uint32_t, 3> deltas{};
            for (auto& value : function) Check(bool(std::cin >> value), "eight scalar words");
            for (auto& value : deltas) Check(bool(std::cin >> value), "three delta words");
            spFunctionEval::SharedRandomForAnalysis().Seed(5489);
            spAnimationManager manager;
            spLightData light;
            light.SetColorForAnalysis({.125F, .25F, .5F, 1.F});
            Check(light.UpdateWorldForAnalysis(), "initial light world");
            Check(light.GetFlagsForAnalysis() == 0x70A00, "qualified initial light flags");
            spLightController controller;
            controller.SetEnabledForAnalysis(enabled != 0);
            controller.SetLightForAnalysis(borrowed ? &light : nullptr);
            auto& color = controller.GetColorEvaluatorForAnalysis();
            color.SetColorsForAnalysis(first, second);
            spFunctionEval::StateForAnalysis state;
            state.time = Float(function[0]); state.frequency = Float(function[1]);
            state.reciprocal = Float(function[2]); state.amplitude = Float(function[3]);
            state.xOffset = Float(function[4]); state.yOffset = Float(function[5]);
            state.pitch = Float(function[6]); state.clampLimit = Float(function[7]);
            state.clampEnabled = clamp != 0; state.functionType = type;
            color.GetFunctionForAnalysis().SetStateForAnalysis(state);
            std::cout << '[';
            for (std::size_t step = 0; step < deltas.size(); ++step)
            {
                Check(controller.TryApplyForAnalysis(Float(deltas[step])), "qualified direct update");
                if (step) std::cout << ',';
                std::cout << '[';
                for (auto value : light.GetColorForAnalysis()) std::cout << Bits(value) << ',';
                std::cout << Bits(color.GetFunctionForAnalysis().GetStateForAnalysis().time)
                          << ',' << light.GetFlagsForAnalysis() << ']';
            }
            std::cout << "]\n";
        }
    }

    void DefaultsAndOwnership()
    {
        spAnimationManager manager;
        spLightData light;
        auto source = std::make_unique<spLightController>();
        Check(source->IsExactly(spLightController::ClassID) && source->IsKindOf(spController::ClassID),
              "original LightController identity and physical base");
        Check(manager.GetControllerCountForAnalysis() == 1, "constructor registers one controller");
        Check(source->IsEnabledForAnalysis() && source->GetLightForAnalysis() == nullptr,
              "enabled and borrowed-light defaults");
        auto& color = source->GetColorEvaluatorForAnalysis();
        Check(color.GetColor1ForAnalysis() == 0xFF000000 && color.GetColor2ForAnalysis() == 0xFF000000,
              "PC black endpoint defaults");
        const auto state = color.GetFunctionForAnalysis().GetStateForAnalysis();
        Check(state.time == 0 && state.frequency == 1 && state.reciprocal == 1 &&
              state.amplitude == 1 && state.xOffset == 0 && state.yOffset == 0 &&
              state.pitch == 0 && state.clampLimit == 0 && !state.clampEnabled && state.functionType == 0,
              "embedded scalar defaults");
        source->SetLightForAnalysis(&light);
        source->SetEnabledForAnalysis(false);
        color.SetColorsForAnalysis(0x80402010, 0xFF112244);
        auto changed = state; changed.time = 3; changed.yOffset = .5F; changed.functionType = 8;
        color.GetFunctionForAnalysis().SetStateForAnalysis(changed);
        const auto before = light.GetColorForAnalysis();
        Check(manager.AdvanceFrameForAnalysis(.25F), "animation frame with disabled controller");
        Check(light.GetColorForAnalysis() == before && color.GetFunctionForAnalysis().GetStateForAnalysis().time == 3,
              "manager applies enabled gate before direct method");
        Check(source->TryApplyForAnalysis(.25F), "direct method executes while disabled");
        Check(light.GetColorForAnalysis() != before && (light.GetFlagsForAnalysis() & 8) != 0,
              "direct method replaces color and marks light data dirty");
        auto cloneOwner = source->Clone();
        const auto* clone = dynamic_cast<const spLightController*>(cloneOwner.get());
        Check(clone && !clone->IsEnabledForAnalysis() && clone->GetLightForAnalysis() == nullptr,
              "clone copies controller gate but resets borrowed light");
        Check(clone->GetColorEvaluatorForAnalysis().GetColor1ForAnalysis() == 0xFF000000 &&
              clone->GetColorEvaluatorForAnalysis().GetFunctionForAnalysis().GetStateForAnalysis().time == 0,
              "clone resets own embedded evaluator");
        Check(manager.GetControllerCountForAnalysis() == 2, "clone registers a second controller");
        cloneOwner.reset(); source.reset();
        Check(manager.GetControllerCountForAnalysis() == 0 && light.GetColorForAnalysis() != before,
              "release unregisters both controllers and preserves borrowed light");
    }

    void HostFiniteGuardAndNullBranch()
    {
        spLightController controller;
        auto state = controller.GetColorEvaluatorForAnalysis().GetFunctionForAnalysis().GetStateForAnalysis();
        state.yOffset = std::numeric_limits<float>::quiet_NaN();
        controller.GetColorEvaluatorForAnalysis().GetFunctionForAnalysis().SetStateForAnalysis(state);
        Check(controller.TryApplyForAnalysis(std::numeric_limits<float>::infinity()),
              "null borrowed light returns before scalar evaluation");
        spLightData light;
        const auto previous = light.GetColorForAnalysis();
        controller.SetLightForAnalysis(&light);
        Check(!controller.TryApplyForAnalysis(0) && light.GetColorForAnalysis() == previous,
              "explicit host guard rejects unsupported scalar without writing light");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch") { RunBatch(); return 0; }
        DefaultsAndOwnership(); HostFiniteGuardAndNullBranch();
        std::cout << "PASS " << checks << '/' << checks << ": PC light-controller runtime and ownership\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
