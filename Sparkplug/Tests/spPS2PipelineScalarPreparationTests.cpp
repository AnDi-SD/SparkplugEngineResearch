#include "Analysis/PS2/spPS2PipelineScalarPreparation.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace sparkplug::evidence::ps2;
    using Input = PipelineScalarInputForAnalysis;
    using Vector = PipelineScalarWordVectorForAnalysis;
    using Stream = std::array<Vector, 4>;
    unsigned checks = 0;

    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }

    std::uint32_t Bits(float value)
    {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    float Single(std::uint32_t value)
    {
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    struct Operation final
    {
        char kind;
        std::uint32_t first;
        std::uint32_t second;
    };

    struct RecordedArithmetic final
    {
        std::vector<Operation> calls;
        std::uint32_t DivideSingle(std::uint32_t first, std::uint32_t second)
        {
            calls.push_back({'D', first, second});
            return 0xA0000000U + static_cast<std::uint32_t>(calls.size());
        }
        std::uint32_t MultiplySingle(std::uint32_t first, std::uint32_t second)
        {
            calls.push_back({'M', first, second});
            return 0xA0000000U + static_cast<std::uint32_t>(calls.size());
        }
    };

    // These exact binary32 examples check the expression wiring, not general EE
    // floating-point equivalence. Native DIV.S/MUL.S are not executed here.
    struct ExactHostExamples final
    {
        std::uint32_t DivideSingle(std::uint32_t first, std::uint32_t second)
        { return Bits(Single(first) / Single(second)); }
        std::uint32_t MultiplySingle(std::uint32_t first, std::uint32_t second)
        { return Bits(Single(first) * Single(second)); }
    };

    Stream Sentinel()
    {
        Stream result;
        for (auto& row : result) row.fill(0xDEADBEEFU);
        return result;
    }
}

int main()
{
    try
    {
        Input source{{0x80000000U, 0x7FC01234U, 0x12345678U},
                     {0U, 1U, 2U, 3U}, 0x40000000U};
        {
            auto first = Sentinel(), second = Sentinel();
            const auto untouched = first;
            std::uint32_t count = 17, mirror = 99;
            RecordedArithmetic arithmetic;
            PreparePipelineScalarsForAnalysis({nullptr, &source, &source, &source},
                first, second, count, mirror, arithmetic);
            Require(arithmetic.calls.empty(), "first NULL prevents all arithmetic");
            Require(first == untouched && second == untouched, "first NULL preserves both streams");
            Require(count == 17 && mirror == 17, "first NULL still mirrors existing count");
        }
        {
            auto first = Sentinel(), second = Sentinel();
            const auto untouched = first;
            std::uint32_t count = 0xFFFFFFFFU, mirror = 99;
            RecordedArithmetic arithmetic;
            PreparePipelineScalarsForAnalysis({&source, nullptr, &source, &source},
                first, second, count, mirror, arithmetic);
            Require(count == 0 && mirror == 0, "increment wraps low32; mirror receives wrapped value");
            Require(arithmetic.calls.size() == 5, "NULL hole terminates rather than skipping");
            Require(first[0] == Vector{0x80000000U, 0x7FC01234U, 0x12345678U, 0xA0000001U},
                "three source words are copied unchanged and fourth comes from divide provider");
            Require(second[0] == Vector{0xA0000002U, 0xA0000003U, 0xA0000004U, 0xA0000005U},
                "four scaled lanes retain provider results without clamping or packing");
            for (unsigned index = 1; index < first.size(); ++index)
                Require(first[index] == untouched[index] && second[index] == untouched[index],
                    "all records from NULL onward remain untouched");
            Require(arithmetic.calls[0].kind == 'D' && arithmetic.calls[0].first == 0xBF800000U
                && arithmetic.calls[0].second == source.fieldEC, "divide operands are minus one and fieldEC");
            for (unsigned component = 0; component < 4; ++component)
                Require(arithmetic.calls[component + 1].kind == 'M'
                    && arithmetic.calls[component + 1].first == 0x437F0000U
                    && arithmetic.calls[component + 1].second == source.fieldD0[component],
                    "multiply provider receives255 and the original lane in order");
        }
        {
            std::array<Input, 4> sources{};
            for (unsigned index = 0; index < sources.size(); ++index)
            {
                sources[index].field70 = {index, index + 10U, index + 20U};
                sources[index].fieldEC = 0x1000U + index;
                sources[index].fieldD0 = {index + 100U, index + 200U, index + 300U, index + 400U};
            }
            auto first = Sentinel(), second = Sentinel();
            std::uint32_t count = 7, mirror = 0;
            RecordedArithmetic arithmetic;
            PreparePipelineScalarsForAnalysis({&sources[0], &sources[1], &sources[2], &sources[3]},
                first, second, count, mirror, arithmetic);
            Require(count == 11 && mirror == 11 && arithmetic.calls.size() == 20,
                "exactly four nonnull slots are processed; count is accumulated rather than reset");
            for (unsigned index = 0; index < sources.size(); ++index)
                Require(first[index][0] == index && first[index][1] == index + 10U
                    && first[index][2] == index + 20U && first[index][3] == 0xA0000001U + index * 5U
                    && second[index][3] == 0xA0000005U + index * 5U,
                    "each source maps to its corresponding16-byte output records");
        }
        {
            Input exact{{Bits(1.0f), Bits(-2.0f), Bits(3.0f)},
                        {Bits(1.0f), Bits(0.5f), Bits(-2.0f), Bits(0.0f)}, Bits(2.0f)};
            auto first = Sentinel(), second = Sentinel();
            std::uint32_t count = 0, mirror = 99;
            ExactHostExamples arithmetic;
            PreparePipelineScalarsForAnalysis({&exact, nullptr, nullptr, nullptr},
                first, second, count, mirror, arithmetic);
            Require(first[0][3] == Bits(-0.5f), "exact host example preserves negative reciprocal");
            Require(second[0] == Vector{Bits(255.0f), Bits(127.5f), Bits(-510.0f), Bits(0.0f)},
                "exact host examples preserve negative and greater-than-one values without saturation");
        }
        std::printf("PS2 pipeline scalar analytical checks passed: %u (no original EE execution)\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "PS2 pipeline scalar analytical failure: %s\n", error.what());
        return 1;
    }
}
