#include "Analysis/PS2/spPS2MeshStripPreparation.h"
#include "Code/Sparkplug/spPS2MeshData.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace sparkplug::evidence::ps2;
    using Strips = std::vector<std::vector<std::uint16_t>>;
    unsigned checks = 0;
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    void Emit(const Strips& source, std::uint32_t capacity,
        const MeshStripPreparationResultForAnalysis& result, bool comma)
    {
        std::printf("%s{\"capacity\":%u,\"input\":[", comma ? "," : "", capacity);
        for (std::size_t at = 0; at < source.size(); ++at)
        {
            std::printf("%s[", at ? "," : "");
            for (std::size_t vertex = 0; vertex < source[at].size(); ++vertex)
                std::printf("%s%u", vertex ? "," : "", static_cast<unsigned>(source[at][vertex]));
            std::printf("]");
        }
        std::printf("],\"output\":[");
        for (std::size_t at = 0; at < result.strips.size(); ++at)
        {
            const auto& strip = result.strips[at];
            std::printf("%s{\"ordinal\":%u,\"indices\":[", at ? "," : "", strip.sourceOrdinal);
            for (std::size_t vertex = 0; vertex < strip.indices.size(); ++vertex)
                std::printf("%s%u", vertex ? "," : "", static_cast<unsigned>(strip.indices[vertex]));
            std::printf("],\"adc\":[");
            for (std::size_t vertex = 0; vertex < strip.adc.size(); ++vertex)
                std::printf("%s%u", vertex ? "," : "", static_cast<unsigned>(strip.adc[vertex]));
            std::printf("]}");
        }
        std::printf("]}");
    }
    std::uint32_t Word(const std::vector<std::byte>& bytes, std::size_t at)
    {
        Require(at + 4 <= bytes.size(), "payload word in bounds");
        std::uint32_t value = 0;
        for (unsigned lane = 0; lane < 4; ++lane)
            value |= std::uint32_t{std::to_integer<unsigned char>(bytes[at + lane])} << (lane * 8);
        return value;
    }
    void PayloadChecks(bool emit)
    {
        using namespace sparkplug::reconstruction;
        using Status = spPS2MeshData::PacketBuildStatusForAnalysis;
        spVertexBuffer vertices;
        std::vector<std::byte> data;
        std::vector<std::uint32_t> inputWords;
        for (unsigned vertex = 0; vertex < 16; ++vertex)
            for (auto word : {vertex * 10U + 1, vertex * 10U + 2, vertex * 10U + 3,
                0xA1B2C300U | vertex, 0x3F000000U + vertex, 0x40000000U + vertex})
            {
                inputWords.push_back(word);
                for (unsigned lane = 0; lane < 4; ++lane)
                    data.push_back(static_cast<std::byte>((word >> (lane * 8)) & 255U));
            }
        Require(vertices.InitializeFromDataForAnalysis(0x900, 16, 0, data), "direct payload normalized VB");
        spIndexBuffer triangle;
        Require(triangle.InitializeForAnalysis(1, spIndexBuffer::eIndexBufferType::Type2), "captured plan input");
        const auto plan = spPS2MeshData::PlanPreparationForAnalysis(triangle, vertices);
        spPS2MeshData mesh;
        const Strips input{{0, 1, 2}, {3, 4, 5, 6}, {7, 8, 9, 10, 11}, {12, 13, 14, 15}};
        if (emit) std::printf("{\"payloads\":[");
        unsigned fixture = 0;
        for (auto capacity : {8U, 6U})
            for (auto grouping : {1U, 0U})
            {
                const auto prepared = PreparePrepopulatedMeshStripsForAnalysis(input, capacity);
                const auto result = mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, plan, input, capacity, grouping);
                Require(result.completed && result.status == Status::Built && result.emittedVertices == 16,
                    "whole direct captured strip payload");
                std::size_t cursor = 0;
                for (const auto& strip : prepared.strips)
                {
                    const auto n = strip.indices.size();
                    const auto aligned = [](std::size_t bytes) { return (bytes + 15U) & ~std::size_t{15}; };
                    const auto position = grouping == 1 ? cursor + 48 + aligned(n * 8) + aligned(n * 4) : cursor + 48;
                    for (std::size_t at = 0; at < n; ++at)
                    {
                        const auto start = position + at * (grouping == 1 ? 16U : 48U);
                        Require(Word(result.bytes, start) == strip.indices[at] * 10U + 1, "payload follows prepared index order");
                        Require(Word(result.bytes, start + 12) == (strip.adc[at] ? 0x8000U : 0U),
                            "position fourth lane alone carries ADC bit15");
                    }
                    cursor += grouping == 1 ? 64 + aligned(n * 8) + aligned(n * 4) + n * 16 : 32 + n * 48;
                    Require(Word(result.bytes, cursor - (grouping == 1 ? 12U : 8U)) == (0x04000000U | n),
                        "one trailer count per surviving strip");
                }
                Require(cursor == result.bytes.size() && !mesh.HasPreparedPacketForAnalysis()
                    && mesh.PacketBytesForAnalysis().empty(), "payload omits RET and does not publish outer packet state");
                if (emit)
                {
                    std::printf("%s{\"capacity\":%u,\"grouping\":%u,\"sourceFlags\":2304,\"strideWords\":6,\"inputVertexWords\":[",
                        fixture ? "," : "", capacity, grouping);
                    for (std::size_t at = 0; at < inputWords.size(); ++at) std::printf("%s%u", at ? "," : "", inputWords[at]);
                    std::printf("],\"input\":[[0,1,2],[3,4,5,6],[7,8,9,10,11],[12,13,14,15]],\"payload\":\"");
                    for (auto byte : result.bytes) std::printf("%02X", std::to_integer<unsigned>(byte));
                    std::printf("\",\"emittedVertices\":%u}", result.emittedVertices);
                }
                ++fixture;
            }
        auto wrong = plan; wrong.field20 = 7;
        Require(!mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, wrong, input, 8).completed,
            "inconsistent captured stream count rejected");
        wrong = plan; wrong.requiresExpandedVertexBuffer = true;
        Require(!mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, wrong, input, 8).completed,
            "direct entry does not silently normalize");
        Require(!mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, plan, {{16, 0, 1}}, 8).completed,
            "out of range direct index rejected");
        Require(!mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, plan, {}, 8).completed,
            "empty direct allocation domain unqualified");
        Require(!mesh.BuildPrepopulatedStripPayloadForAnalysis(vertices, plan, input, 8, 1, 255).completed,
            "direct staging/output obey byte budget");
        Require(vertices.GetDataForAnalysis() == data && mesh.PacketBytesForAnalysis().empty(),
            "direct failure leaves input and outer state unchanged");
        if (emit) std::printf("],\"checks\":%u}\n", checks);
    }
}

int main(int argc, char** argv)
{
    try
    {
        const bool emit = argc == 2 && std::strcmp(argv[1], "--strip-fixtures") == 0;
        const bool emitPayload = argc == 2 && std::strcmp(argv[1], "--strip-payload-fixtures") == 0;
        if (emit) std::printf("{\"fixtures\":[");
        const std::vector<Strips> sources{
            {},
            {{0, 1, 2}},
            {{0, 1, 2, 3}, {4, 5, 6, 7}},
            {{0, 1, 2}, {3, 4, 5}},
            {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}},
            {{0, 1, 2}, {3, 4, 5, 6}, {7, 8, 9}, {10, 11, 12, 13}},
            {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}},
            {{0, 1, 2, 3, 4}, {5, 6, 7}, {8, 9, 10}, {11, 12, 13, 14, 15}},
            {{0, 1, 2}, {3, 4, 5, 6}, {7, 8, 9, 10, 11}, {12, 13, 14, 15}},
            {{0, 1, 2}, {3, 4, 5, 6}, {7, 8, 9, 10, 11}, {12, 13, 14, 15}},
        };
        const std::vector<std::uint32_t> capacities{9, 9, 9, 9, 9, 9, 6, 9, 8, 6};
        const std::vector<Strips> expected{
            {}, {{0, 1, 2}}, {{0, 1, 2, 3, 4, 5, 6, 7}},
            {{0, 1, 2, 5, 4, 3}}, {{0, 1, 2, 5, 4, 3, 6, 7, 8}},
            {{3, 4, 5, 6, 10, 11, 12, 13}, {0, 1, 2, 9, 8, 7}},
            {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}},
            {{0, 1, 2, 3, 4, 7, 6, 5}, {8, 9, 10, 15, 14, 13, 12, 11}},
            {{3, 4, 5, 6, 12, 13, 14, 15}, {0, 1, 2, 11, 10, 9, 8, 7}},
            {{7, 8, 9, 10, 11}, {3, 4, 5, 6}, {12, 13, 14, 15}, {0, 1, 2}},
        };
        const std::vector<std::vector<std::vector<std::uint8_t>>> expectedAdc{
            {}, {{0, 0, 0}}, {{0, 0, 0, 0, 1, 1, 0, 0}},
            {{0, 0, 0, 1, 1, 0}}, {{0, 0, 0, 1, 1, 0, 1, 1, 0}},
            {{0, 0, 0, 0, 1, 1, 0, 0}, {0, 0, 0, 1, 1, 0}},
            {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
            {{0, 0, 0, 0, 0, 1, 1, 0}, {0, 0, 0, 1, 1, 0, 0, 0}},
            {{0, 0, 0, 0, 1, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}},
            {{0, 0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0}},
        };
        for (std::size_t at = 0; at < sources.size(); ++at)
        {
            const auto original = sources[at];
            const auto result = PreparePrepopulatedMeshStripsForAnalysis(sources[at], capacities[at]);
            Require(result.completed && result.status == MeshStripPreparationStatusForAnalysis::Prepared, "supported strip case");
            Require(sources[at] == original, "input records unchanged");
            Require(result.strips.size() == expected[at].size(), "survivor count");
            for (std::size_t strip = 0; strip < result.strips.size(); ++strip)
            {
                Require(result.strips[strip].indices == expected[at][strip], "directed parity/order golden");
                Require(result.strips[strip].adc.size() == result.strips[strip].indices.size(), "one ADC byte per vertex");
                Require(result.strips[strip].adc == expectedAdc[at][strip], "literal ADC byte golden");
            }
            for (const auto& join : result.joins)
            {
                bool found = false;
                for (const auto& strip : result.strips) if (strip.sourceOrdinal == join.anchorOrdinal)
                {
                    Require(strip.adc[join.firstAppendedVertex] == 1 && strip.adc[join.firstAppendedVertex + 1] == 1,
                        "first two appended ADC set");
                    Require(join.reversed == ((join.firstAppendedVertex & 1U) != 0), "join parity recorded");
                    found = true;
                }
                Require(found, "join anchor retained");
            }
            if (at == 6)
                Require(result.strips[0].sourceOrdinal == 0 && result.strips[1].sourceOrdinal == 1
                    && result.strips[2].sourceOrdinal == 2, "equal-size stable order");
            if (at == 4)
                Require(result.strips[0].adc == std::vector<std::uint8_t>{0, 0, 0, 1, 1, 0, 1, 1, 0}, "two join ADC golden");
            if (at == 8)
                Require(result.strips[0].sourceOrdinal == 1 && result.strips[1].sourceOrdinal == 0,
                    "equal joined sizes preserve even-before-odd order");
            if (at == 9)
                Require(result.strips[0].sourceOrdinal == 2 && result.strips[1].sourceOrdinal == 1
                    && result.strips[2].sourceOrdinal == 3 && result.strips[3].sourceOrdinal == 0,
                    "four-survivor recursive sort preserves equal even order");
            if (emit) Emit(sources[at], capacities[at], result, at != 0);
        }
        const auto badCapacity = PreparePrepopulatedMeshStripsForAnalysis({}, 2);
        Require(!badCapacity.completed && badCapacity.status == MeshStripPreparationStatusForAnalysis::InvalidCapacity, "capacity guard");
        const auto small = PreparePrepopulatedMeshStripsForAnalysis({{0, 1}}, 9);
        Require(!small.completed && small.status == MeshStripPreparationStatusForAnalysis::InvalidStripCount, "small count explicit unknown");
        const auto oversized = PreparePrepopulatedMeshStripsForAnalysis({{0, 1, 2, 3}}, 3);
        Require(!oversized.completed && oversized.status == MeshStripPreparationStatusForAnalysis::StripExceedsCapacity, "oversized strip explicit unknown");
        const auto budget = PreparePrepopulatedMeshStripsForAnalysis({{0, 1, 2}}, 9, 8);
        Require(!budget.completed && budget.status == MeshStripPreparationStatusForAnalysis::ByteBudgetExceeded, "logical byte budget");
        Require(budget.strips.empty() && budget.joins.empty(), "refusal publishes no partial result");
        if (emit) std::printf("],\"checks\":%u}\n", checks);
        PayloadChecks(emitPayload);
        if (!emit && !emitPayload) std::printf("PASS: %u PS2 strip preparation checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
