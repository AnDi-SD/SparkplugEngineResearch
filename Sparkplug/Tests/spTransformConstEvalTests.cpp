#include "Code/Sparkplug/spTransformConstEval.h"
#include "Analysis/PC/spTransformConstMath.h"
#include "Analysis/PC/spTransformConstEvalAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    using Eval = spTransformConstEval;
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t result;
        std::memcpy(&result, &value, 4);
        return result;
    }
    float ReadFloat(std::istream& in)
    {
        std::uint32_t bits;
        if (!(in >> bits))
            throw std::runtime_error("Missing float bits");
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    void Case(const std::string& line)
    {
        std::istringstream in(line);
        Eval evaluator;
        auto state = evaluator.GetStateForAnalysis();
        unsigned position, rotation;
        if (!(in >> position >> rotation))
            throw std::runtime_error("Missing selectors");
        state.positionEnabled = std::uint8_t(position);
        state.rotationEnabled = std::uint8_t(rotation);
        const float time = ReadFloat(in);
        for (auto& value : state.velocity)
            value = ReadFloat(in);
        for (auto& value : state.axis)
            value = ReadFloat(in);
        state.angle = ReadFloat(in);
        Eval::SampleForAnalysis sample;
        for (auto& value : sample.position)
            value = ReadFloat(in);
        for (auto& value : sample.rotation)
            value = ReadFloat(in);
        for (auto& value : sample.scale)
            value = ReadFloat(in);
        unsigned valid[3];
        if (!(in >> valid[0] >> valid[1] >> valid[2]))
            throw std::runtime_error("Missing validity seeds");
        sample.hasPosition = valid[0] != 0;
        sample.hasRotation = valid[1] != 0;
        sample.hasScale = valid[2] != 0;
        auto& function = state.scaleFunction;
        function.time = ReadFloat(in);
        function.frequency = ReadFloat(in);
        function.reciprocal = ReadFloat(in);
        function.amplitude = ReadFloat(in);
        function.xOffset = ReadFloat(in);
        function.yOffset = ReadFloat(in);
        function.pitch = ReadFloat(in);
        function.clampLimit = ReadFloat(in);
        unsigned clamp;
        if (!(in >> clamp >> function.functionType))
            throw std::runtime_error("Missing function selectors");
        function.clampEnabled = clamp != 0;
        std::string extra;
        if (in >> extra)
            throw std::runtime_error("Trailing case fields");
        evaluator.SetStateForAnalysis(state);
        if (!evaluator.EvaluateInPlaceForAnalysis(time, sample))
            throw std::runtime_error("Unsupported native case");
        std::cout << '[' << sample.hasPosition << ',' << sample.hasRotation << ',' << sample.hasScale;
        for (float value : sample.position)
            std::cout << ',' << Bits(value);
        for (float value : sample.rotation)
            std::cout << ',' << Bits(value);
        for (float value : sample.scale)
            std::cout << ',' << Bits(value);
        std::cout << ',' << Bits(evaluator.GetStateForAnalysis().scaleFunction.time) << "]\n";
    }
    void MainChecks()
    {
        Eval evaluator;
        auto state = evaluator.GetStateForAnalysis();
        Check(state.scaleFunction.functionType == 0 && state.velocity == Eval::Vector3{} &&
            state.axis == Eval::Vector3{0, 0, 1} && state.angle == 0 &&
            state.positionEnabled == 0 && state.rotationEnabled == 0,
            "native constructor initializes embedded function type and own PR/flags");
        auto sample = Eval::SampleForAnalysis{};
        Check(evaluator.EvaluateInPlaceForAnalysis(0, sample), "default native channels are disabled");
        evaluator.SetStateForAnalysis(state);
        sample.position = {10, 20, 30};
        sample.rotation = {1, 2, 3, 4};
        sample.scale = {2, 3, 4};
        sample.hasPosition = sample.hasRotation = sample.hasScale = true;
        Check(evaluator.EvaluateInPlaceForAnalysis(0.5F, sample), "disabled operation");
        Check(sample.position == Eval::Vector3{10, 20, 30} &&
            sample.rotation == Eval::Quaternion{1, 2, 3, 4} &&
            sample.scale == Eval::Vector3{2, 3, 4} &&
            !sample.hasPosition && !sample.hasRotation && !sample.hasScale,
            "disabled channels preserve caller payload and replace flags");
        state.positionEnabled = 255;
        state.velocity = {1, 2, 4};
        evaluator.SetStateForAnalysis(state);
        Check(evaluator.EvaluateInPlaceForAnalysis(0.5F, sample) &&
            sample.position == Eval::Vector3{10.5F, 21, 32} && sample.hasPosition,
            "translation adds to incoming position with raw nonzero selector");
        state.positionEnabled = 0;
        state.rotationEnabled = 255;
        state.scaleFunction.functionType = 7;
        state.scaleFunction.yOffset = 7;
        evaluator.SetStateForAnalysis(state);
        Check(evaluator.EvaluateInPlaceForAnalysis(-2, sample) &&
            sample.rotation == Eval::Quaternion{1, 2, 3, 4} &&
            sample.scale == Eval::Vector3{7, 7, 7} && sample.hasRotation && sample.hasScale,
            "angle zero is independent of time; function type7 selects uniform scale");
        spCloneManager clones;
        auto clone = clones.Clone(evaluator);
        auto* typed = dynamic_cast<Eval*>(clone.get());
        Check(typed && typed->GetStateForAnalysis().scaleFunction.functionType == 0 &&
            typed->GetStateForAnalysis().rotationEnabled == 0 &&
            typed->GetStateForAnalysis().scaleFunction.yOffset == 0,
            "clone resets own payload through factory and inherited base copy");
        state.scaleFunction.functionType = 0;
        state.positionEnabled = 1;
        state.rotationEnabled = 0;
        state.velocity[0] = std::numeric_limits<float>::infinity();
        evaluator.SetStateForAnalysis(state);
        const auto before = sample.position;
        Check(!evaluator.EvaluateInPlaceForAnalysis(1, sample) && sample.position == before,
            "host finite preflight preserves input on failure");
        using namespace sparkplug::evidence::pc::transform_const_math;
        const auto lowerPowerTie = AddNearest({std::uint64_t(1) << 63, 0, false},
            {(std::uint64_t(1) << 63) + 1, -65, true});
        Check(lowerPowerTie.mantissa == ~std::uint64_t(0) && lowerPowerTie.exponent == -1,
            "extended spacing below exact power of two at exponent gap65");
        Check(Bits(AddProduct(-0.0F, 0, 1, false)) == 0 &&
            Bits(AddProduct(-0.0F, -0.0F, 1, false)) == 0x80000000U,
            "round-to-nearest signed-zero addition");
        // Exact product lies just below a halfway float. X keeps this residue,
        // while Y/Z round the product before adding the baseline.
        Check(Bits(AddProduct(-1, 1.00000011920928955078125F,
            0.999999940395355224609375F, false)) == 0x337FFFFE &&
            Bits(AddProduct(-1, 1.00000011920928955078125F,
            0.999999940395355224609375F, true)) == 0,
            "original X/YZ product spill difference");
        Check(evaluator.vfunc_18().IsExactly(Eval::ClassID) &&
            evaluator.vfunc_18().IsKindOf(spTransformEval::ClassID), "original RTTI ancestry");
        std::cout << "PASS " << checks << "/" << checks << ": transform-const lifecycle and incoming PRS\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
        {
            std::string line;
            while (std::getline(std::cin, line))
                if (!line.empty())
                    Case(line);
        }
        else
            MainChecks();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
