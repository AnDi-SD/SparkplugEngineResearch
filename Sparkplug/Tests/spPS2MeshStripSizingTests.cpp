#include "Analysis/PS2/spPS2MeshStripSizing.h"
#include "Analysis/PS2/spPS2MeshPacketFormat.h"
#include "Code/Sparkplug/spPS2MeshData.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace
{
    using namespace sparkplug::evidence::ps2;
    unsigned checks = 0;
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    struct Fixture
    {
        const char* name;
        MeshStripSizingInputsForAnalysis input;
        std::uint32_t expectedQwords;
        std::array<std::uint32_t, 22> expectedCounters{};
    };
    Fixture Case(const char* name, std::uint32_t flags, std::uint32_t streams,
        std::uint32_t grouping, std::uint32_t declared, std::uint32_t a2,
        std::initializer_list<std::uint16_t> counts, std::uint32_t qwords,
        std::initializer_list<std::pair<unsigned, std::uint32_t>> counters = {})
    {
        Fixture f{name, {}, qwords, {}};
        f.input.effectiveFlags = flags; f.input.streamCount = streams;
        f.input.grouping = grouping; f.input.declaredStripCount = declared; f.input.argumentA2 = a2;
        f.input.traversedCounts = counts;
        // Test inputs use the recovered constructor table. The sizing helper
        // itself accepts captured fields and never constructs a native object.
        sparkplug::reconstruction::spPS2MeshData defaults;
        f.input.descriptors = defaults.GetField48ForAnalysis();
        for (auto [component, bytes] : counters) f.expectedCounters[component] = bytes;
        return f;
    }
    void Print(const Fixture& f, const MeshStripSizingResultForAnalysis& r, bool comma)
    {
        std::printf("%s{\"name\":\"%s\",\"effectiveFlags\":%u,\"streamCount\":%u,\"grouping\":%u,\"declaredStripCount\":%u,\"argumentA2\":%u,\"descriptors\":[",
            comma ? "," : "", f.name, f.input.effectiveFlags, f.input.streamCount,
            f.input.grouping, f.input.declaredStripCount, f.input.argumentA2);
        for (std::size_t at = 0; at < f.input.descriptors.size(); ++at)
            std::printf("%s%u", at ? "," : "", f.input.descriptors[at]);
        std::printf("],\"traversedCounts\":[");
        for (std::size_t at = 0; at < f.input.traversedCounts.size(); ++at)
            std::printf("%s%u", at ? "," : "", static_cast<unsigned>(f.input.traversedCounts[at]));
        std::printf("],\"qwords\":%u,\"componentCountersProduced\":%s,\"recordsVisited\":%u,\"alignedComponentBytes\":[",
            r.qwords, r.componentCountersProduced ? "true" : "false", r.recordsVisited);
        for (std::size_t at = 0; at < r.alignedComponentBytes.size(); ++at)
            std::printf("%s%u", at ? "," : "", r.alignedComponentBytes[at]);
        std::printf("]}");
    }
}

int main(int argc, char** argv)
{
    try
    {
        const bool emit = argc == 2 && std::strcmp(argv[1], "--sizing-fixtures") == 0;
        std::vector<Fixture> fixtures{
            Case("inter-capacity", 0x900, 3, 0, 4, 40, {3,4,5,4}, 128),
            Case("inter-nonone-mismatch", 0x900, 3, 99, 2, 16, {3}, 52),
            Case("grouped-default", 0x900, 3, 1, 2, 1234, {3,5}, 24, {{0,128},{9,48},{12,80}}),
            Case("grouped-declared-mismatch", 0x900, 3, 1, 7, 0, {3,5}, 44, {{0,128},{9,48},{12,80}}),
            Case("grouped-weights", 0x922, 5, 1, 2, 0, {3,5}, 44, {{0,128},{2,128},{6,128},{9,48},{12,80}}),
            Case("grouped-compressed-normal-size-only", 0x840, 3, 1, 2, 0, {3,5}, 23, {{0,128},{7,32},{12,80}}),
            Case("grouped-format68-unknownUV", 0x900, 3, 1, 1, 0, {5}, 10, {{0,64},{9,32}}),
            Case("grouped-skip-weight-lanes", 0x93E, 5, 1, 1, 0, {1}, 11, {{0,16},{2,16},{6,16},{9,16},{12,16}}),
            Case("inter-low32-declared-shift-wrap", 0x900, 3, 0, 0x80000001U, 7, {}, 23),
            Case("grouped-low32-add-wrap", 0, 1, 1, 0x7FFFFFFFU, 99, {2}, 0, {{0,32}}),
        };
        fixtures[6].input.descriptors[0] = 0xABCD0068;
        fixtures[6].input.descriptors[12] = 0x77;
        if (emit) std::printf("{\"fixtures\":[");
        for (std::size_t at = 0; at < fixtures.size(); ++at)
        {
            const auto& f = fixtures[at];
            const auto r = ComputeMeshStripReservationForAnalysis(f.input);
            Require(r.completed && r.status == MeshStripSizingStatusForAnalysis::Complete, "captured sizing completed");
            Require(r.qwords == f.expectedQwords, "literal native reservation QW golden");
            Require(r.componentCountersProduced == (f.input.grouping == 1), "interleaved has no native temporary counters");
            Require(r.recordsVisited == (f.input.grouping == 1 ? f.input.traversedCounts.size() : 0), "traversal independent of declared count");
            Require(r.alignedComponentBytes == f.expectedCounters, "literal22component aligned byte counters");
            if (emit) Print(f, r, at != 0);
        }
        const auto budget = ComputeMeshStripReservationForAnalysis(fixtures[2].input, 1);
        Require(!budget.completed && budget.status == MeshStripSizingStatusForAnalysis::RecordBudgetExceeded
            && !budget.qwords && !budget.componentCountersProduced && !budget.recordsVisited
            && std::all_of(budget.alignedComponentBytes.begin(), budget.alignedComponentBytes.end(), [](auto word) { return !word; }),
            "host record budget refuses before partial output");
        Require(ComputeMeshStripReservationForAnalysis(fixtures[0].input, 0).qwords == 128,
            "interleaved never validates or traverses captured list body");
        auto last = Case("last-component", 0x100000, 2, 1, 1, 0, {1}, 5);
        last.input.descriptors[21] = 0x6C;
        const auto lastResult = ComputeMeshStripReservationForAnalysis(last.input);
        Require(lastResult.completed && lastResult.qwords == 5 && lastResult.alignedComponentBytes[21] == 16,
            "native final sum includes separate22nd counter");
        const auto empty = Case("empty", 0x900, 3, 1, 0, 0, {}, 0);
        Require(ComputeMeshStripReservationForAnalysis(empty.input).qwords == 0, "empty captured grouped list");
        const auto wrappedMultiply = Case("product-wrap", 0, 0xFFFFFFFFU, 0, 1, 2, {}, 0);
        Require(ComputeMeshStripReservationForAnalysis(wrappedMultiply.input).qwords == 0,
            "value API retains low32 multiply result; hardware multiply sideeffects unclaimed");
        const std::pair<std::uint32_t, std::uint32_t> widths[]{{0x64,8},{0x68,12},{0x6C,16},{0x6F,2},{0x6E,4},{0x77,0}};
        for (auto [format, width] : widths)
            Require(MeshPacketComponentByteWidthForAnalysis(0xABCD0000U | format) == width, "shared low16 format width");
        if (emit) std::printf("],\"checks\":%u}\n", checks);
        else std::printf("PASS: %u PS2 strip sizing checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
