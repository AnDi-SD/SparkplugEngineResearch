#include "Analysis/PS2/spPS2MeshNormalPacking.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace sparkplug::evidence::ps2;
    using Normal = std::array<std::uint32_t, 3>;
    unsigned checks = 0;
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    struct RecordedArithmetic final
    {
        std::vector<char> calls;
        unsigned converted = 0;
        bool TryAddSingle(std::uint32_t a, std::uint32_t b, std::uint32_t& out)
        {
            Require(a == 0x3F800000U && b == 12U - converted, "native Z,Y,X order and1 constant");
            calls.push_back('A'); out = 100 + converted; return true;
        }
        bool TryMultiplySingle(std::uint32_t a, std::uint32_t b, std::uint32_t& out)
        {
            Require(a == 0x41780000U && b == 100 + converted, "15.5 constant and sum result");
            calls.push_back('M'); out = 200 + converted; return true;
        }
        bool TryLessEqualSingle(std::uint32_t a, std::uint32_t b, bool& out)
        {
            Require(a == 0x4F000000U && b == 200 + converted, "threshold left operand");
            calls.push_back('C'); out = converted == 0; return true;
        }
        bool TrySubtractSingle(std::uint32_t a, std::uint32_t b, std::uint32_t& out)
        {
            Require(a == 200 && b == 0x4F000000U, "unsigned high branch adjustment");
            calls.push_back('S'); out = 300; return true;
        }
        bool TryConvertToWord(std::uint32_t source, std::uint32_t& out)
        {
            Require(source == (converted == 0 ? 300U : 200 + converted), "conversion operand routing");
            calls.push_back('W'); out = 33 + converted++; return true;
        }
    };
}

int main(int argc, char** argv)
{
    try
    {
        const bool emit = argc == 2 && std::strcmp(argv[1], "--normal-fixtures") == 0;
        ExactMeshNormalArithmeticForAnalysis arithmetic;
        const std::array<Normal, 10> normals{{
            {0, 0, 0}, {0x80000000U, 0, 0x80000000U},
            {0xBF800000U, 0xBF800000U, 0xBF800000U},
            {0x3F800000U, 0x3F800000U, 0x3F800000U},
            {0, 0x3F800000U, 0xBF800000U},
            {0x3F000000U, 0xBF000000U, 0x40000000U},
            {0xC0000000U, 0x40400000U, 0xC0400000U},
            {0x3D800000U, 0xBD800000U, 0x3E800000U},
            {0x3F400000U, 0xBF400000U, 0x3E000000U},
            {0x3FC00000U, 0xBFC00000U, 0x40800000U},
        }};
        const std::array<std::uint16_t, 10> expected{{
            0xBDEF, 0xBDEF, 0x8000, 0xFFFF, 0x83EF,
            0xB8F7, 0x87D1, 0xCDD0, 0xC47B, 0xB726,
        }};
        if (emit) std::printf("{\"fixtures\":[");
        for (std::size_t at = 0; at < normals.size(); ++at)
        {
            const auto result = PackCompressedMeshNormalForAnalysis(normals[at], arithmetic);
            Require(result.completed && result.stage == MeshNormalPackingStageForAnalysis::Complete, "exact representable arithmetic domain");
            Require(result.packed == expected[at], "literal16-bit compressed normal golden");
            if (emit) std::printf("%s{\"normal\":[%u,%u,%u],\"packed\":%u}", at ? "," : "",
                normals[at][0], normals[at][1], normals[at][2], static_cast<unsigned>(result.packed));
        }
        {
            RecordedArithmetic recorded;
            const auto packed = PackCompressedMeshNormalForAnalysis(Normal{10, 11, 12}, recorded);
            Require(packed.completed && packed.packed == 0x8443, "provider low5 lane packing");
            Require(recorded.calls == std::vector<char>{'A', 'M', 'C', 'S', 'W', 'A', 'M', 'C', 'W', 'A', 'M', 'C', 'W'},
                "native primitive call order including unsigned branch");
        }
        for (auto word : {0x7F800000U, 0x7FC01234U, 1U, 0x80000001U})
        {
            const auto result = PackCompressedMeshNormalForAnalysis(Normal{0, 0, word}, arithmetic);
            Require(!result.completed && result.stage == MeshNormalPackingStageForAnalysis::Add && result.component == 2,
                "special or subnormal input refused at firstnative arithmetic");
        }
        {
            const auto result = PackCompressedMeshNormalForAnalysis(Normal{0, 0, 0x33800000U}, arithmetic); //2^-24
            Require(!result.completed && result.stage == MeshNormalPackingStageForAnalysis::Add, "addition requiring rounding refused");
            const auto multiply = PackCompressedMeshNormalForAnalysis(Normal{0, 0, 0x34000000U}, arithmetic); //2^-23
            Require(!multiply.completed && multiply.stage == MeshNormalPackingStageForAnalysis::Multiply, "multiplication requiring rounding refused");
        }
        std::uint32_t word = 0xDEADBEEFU;
        Require(arithmetic.TryAddSingle(0x3F800000U, 0xB3800000U, word) && word == 0x3F7FFFFFU, "exact cancellation near1");
        Require(!arithmetic.TryAddSingle(0x3F800000U, 0x33800000U, word) && word == 0x3F7FFFFFU, "refused arithmetic preserves output");
        Require(arithmetic.TrySubtractSingle(0x4F000001U, 0x4F000000U, word) && word == 0x43800000U, "exact unsigned adjustment subtraction");
        Require(arithmetic.TryConvertToWord(0x40F80000U, word) && word == 7, "positiveCVT truncates");
        Require(arithmetic.TryConvertToWord(0xC0F80000U, word) && word == 0xFFFFFFF9U, "negativeCVT truncates");
        Require(arithmetic.TryConvertToWord(0x4F000000U, word) && word == 0x7FFFFFFFU, "positiveCVT saturates");
        Require(arithmetic.TryConvertToWord(0xCF000000U, word) && word == 0x80000000U, "negativeCVT saturation boundary");
        Require(!arithmetic.TryConvertToWord(0x7F800000U, word) && word == 0x80000000U, "specialCVT refused");
        Require(!arithmetic.TryMultiplySingle(0x7F7FFFFFU, 0x40000000U, word) && word == 0x80000000U,
            "overflowing multiply refused without mutation");
        Require(!arithmetic.TryMultiplySingle(0x00800000U, 0x3F000000U, word) && word == 0x80000000U,
            "subnormal-result multiply refused without mutation");
        Require(!arithmetic.TryAddSingle(0x7F7FFFFFU, 0x3F800000U, word) && word == 0x80000000U,
            "wide-exponent inexact addition refused");
        bool less = false;
        Require(arithmetic.TryLessEqualSingle(0, 0x80000000U, less) && less, "bothzero signs compare equal");
        Require(arithmetic.TryLessEqualSingle(0xBF800000U, 0, less) && less, "negative precedeszero");
        Require(arithmetic.TryLessEqualSingle(0x3F800000U, 0, less) && !less, "positive followszero");
        if (emit) std::printf("],\"checks\":%u}\n", checks);
        else std::printf("PASS: %u PS2 compressed-normal packing checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
