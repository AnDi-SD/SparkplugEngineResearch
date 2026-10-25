#include "Code/Sparkplug/spPS2MeshData.h"
#include "Analysis/PS2/spPS2MeshWeightPacking.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace sparkplug::reconstruction;
    using Status = spPS2MeshData::PacketBuildStatusForAnalysis;
    using Type = spIndexBuffer::eIndexBufferType;
    unsigned checks = 0;
    bool exportFixtures = false;
    std::array<std::vector<std::byte>, 4> actualPacketFixtures;
    bool exportExpandedFixtures = false;
    struct ExpandedFixture
    {
        std::uint32_t sourceFlags = 0, effectiveFlags = 0, strideWords = 0, grouping = 1;
        std::vector<std::byte> source, allocation, normalized, packet;
        std::vector<std::uint8_t> written;
    };
    std::array<ExpandedFixture, 4> actualExpandedFixtures;
    void PrintHex(const std::vector<std::byte>& bytes)
    {
        for (auto byte : bytes) std::printf("%02X", std::to_integer<unsigned>(byte));
    }
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    std::vector<std::byte> Bytes(std::initializer_list<std::uint32_t> words)
    {
        std::vector<std::byte> result;
        for (auto word : words)
            for (unsigned lane = 0; lane < 4; ++lane)
                result.push_back(static_cast<std::byte>((word >> (lane * 8)) & 255U));
        return result;
    }
    std::uint32_t Word(const std::vector<std::byte>& bytes, std::size_t index)
    {
        Require(index < bytes.size() / 4, "test word index in range");
        std::uint32_t result = 0;
        for (unsigned lane = 0; lane < 4; ++lane)
            result |= std::uint32_t{std::to_integer<std::uint8_t>(bytes[index * 4 + lane])} << (lane * 8);
        return result;
    }
    void Triangle(spIndexBuffer& indices)
    {
        Require(indices.InitializeForAnalysis(1, Type::Type2), "triangle initialized");
        Require(indices.SetIndexForAnalysis(0, 2) && indices.SetIndexForAnalysis(1, 0)
            && indices.SetIndexForAnalysis(2, 1), "nonidentity triangle order");
    }
    void Vertices(spVertexBuffer& vertices, bool normal)
    {
        const auto data = normal ? Bytes({
            1, 2, 3, 0x80000000, 0x7FC12345, 0x7F800000, 0xA1B2C3D4, 10, 11,
            4, 5, 6, 0x3F800000, 0, 0xBF800000, 0x10203040, 12, 13,
            7, 8, 9, 0x3E800000, 0xBE800000, 0x00000001, 0xFFEEDDCC, 14, 15}) : Bytes({
            1, 2, 3, 0xA1B2C3D4, 10, 11,
            4, 5, 6, 0x10203040, 12, 13,
            7, 8, 9, 0xFFEEDDCC, 14, 15});
        Require(vertices.InitializeFromDataForAnalysis(normal ? 0x940U : 0x900U, 3, 17, data),
            "raw semantic vertices initialized");
    }
    void Normalization()
    {
        spVertexBuffer compatible;
        Vertices(compatible, true);
        const auto before = compatible.GetDataForAnalysis();
        auto result = spPS2MeshData::NormalizeVertexBufferForAnalysis(compatible, nullptr, 0);
        Require(result.completed && result.borrowed && !result.expanded && result.buffer == &compatible
            && result.writtenBytes.empty() && compatible.GetDataForAnalysis() == before,
            "compatible normalization borrows exact source without allocation");

        spVertexBuffer position;
        Require(position.InitializeFromDataForAnalysis(0, 2, 9,
            Bytes({0x80000000, 0x7FC12345, 0x7F800000, 0x00000001, 0xFF800000, 0x3F800000})),
            "position-only source");
        result = spPS2MeshData::NormalizeVertexBufferForAnalysis(position);
        Require(result.completed && !result.borrowed && result.expanded && result.buffer == result.expanded.get()
            && result.buffer->GetComponentFlagsForAnalysis() == 0x900 && result.buffer->GetFlagsForAnalysis() == 9,
            "normalization owns expanded source and preserves flags");
        Require(result.buffer->GetDataForAnalysis() == Bytes({
            0x80000000, 0x7FC12345, 0x7F800000, 0, 0, 0,
            0x00000001, 0xFF800000, 0x3F800000, 0, 0, 0})
            && std::all_of(result.writtenBytes.begin(), result.writtenBytes.end(), [](auto value) { return value == 1; }),
            "raw float words preserved; default color and UV are exact zero writes");
        Require(position.GetDataForAnalysis() == Bytes({0x80000000, 0x7FC12345, 0x7F800000,
            0x00000001, 0xFF800000, 0x3F800000}), "normalization leaves source unchanged");
        Require(!spPS2MeshData::NormalizeVertexBufferForAnalysis(position, nullptr, 47).completed,
            "expanded allocation obeys byte budget before allocation");
        const std::vector<std::byte> wrongCapture(47);
        Require(!spPS2MeshData::NormalizeVertexBufferForAnalysis(position, &wrongCapture).completed,
            "captured allocation length must be exact");

        spVertexBuffer onlyNormal;
        Require(onlyNormal.InitializeFromDataForAnalysis(0x40, 1, 0, Bytes({1, 2, 3, 4, 5, 6})),
            "normal source missing color and UV");
        result = spPS2MeshData::NormalizeVertexBufferForAnalysis(onlyNormal);
        Require(result.completed && result.buffer->GetComponentFlagsForAnalysis() == 0x840
            && result.buffer->GetDataForAnalysis() == Bytes({0, 2, 3, 4, 5, 6, 0, 0}),
            "native absent-color offset-zero overwrite of position.x retained");

        spVertexBuffer extra;
        Require(extra.InitializeFromDataForAnalysis(3, 1, 0,
            Bytes({1, 2, 3, 0xAAAAAAAA, 0xBBBBBBBB})), "source with RHW and weight");
        const std::vector<std::byte> allocation(32, std::byte{0xCD});
        result = spPS2MeshData::NormalizeVertexBufferForAnalysis(extra, &allocation);
        Require(result.completed && result.capturedAllocationUsed
            && Word(result.buffer->GetDataForAnalysis(), 3) == 0xCDCDCDCD
            && Word(result.buffer->GetDataForAnalysis(), 4) == 0xCDCDCDCD,
            "native unhandled components retain captured allocation rather than copied input");
        Require(std::count(result.writtenBytes.begin(), result.writtenBytes.end(), std::uint8_t{0}) == 8
            && result.writtenBytes[12] == 0 && result.writtenBytes[19] == 0
            && result.writtenBytes[20] == 1 && result.writtenBytes[31] == 1,
            "normalization write mask distinguishes untouched component bytes");

        spVertexBuffer uvOnly;
        Require(uvOnly.InitializeFromDataForAnalysis(0x1800, 1, 0, Bytes({1, 2, 3, 4, 5, 6, 7})),
            "two UV source without alternatives");
        result = spPS2MeshData::NormalizeVertexBufferForAnalysis(uvOnly);
        Require(result.completed && result.buffer->GetDataForAnalysis() == Bytes({1, 2, 3, 0, 4, 5, 6, 7}),
            "both UV channels copied after added color");
        spVertexBuffer uninitialized;
        Require(!spPS2MeshData::NormalizeVertexBufferForAnalysis(uninitialized).completed,
            "uninitialized source remains incomplete");
    }
    void GroupedGolden()
    {
        spIndexBuffer indices; Triangle(indices);
        spVertexBuffer vertices; Vertices(vertices, false);
        spPS2MeshData mesh;
        const auto result = mesh.BuildPacketForAnalysis(indices, vertices);
        Require(result.completed && result.nativeReturn && result.status == Status::Built,
            "grouped triangle complete");
        // Literal EE stores, not a second implementation of the emitter.
        const auto expected = Bytes({
            0x6000000A, 0, 0x11000000, 0x11000000,
            0, 0, 0x01030103, 0x64038000,
            14, 15, 10, 11, 12, 13, 0, 0,
            0, 0, 0x01030103, 0x6E03C001,
            0xFFCCDDEE, 0xA1D4C3B2, 0x10403020, 0,
            0, 0, 0x01030103, 0x6C038002,
            7, 8, 9, 0, 1, 2, 3, 0, 4, 5, 6, 0,
            0, 0x04000003, 0x01010101, 0x17000000});
        Require(mesh.PacketBytesForAnalysis() == expected,
            "whole grouped packet exact headers order payload padding trailer and RET length");
        if (exportFixtures) actualPacketFixtures[0] = mesh.PacketBytesForAnalysis();
        const auto& info = mesh.PacketInfoForAnalysisView();
        Require(info.qwords == 11 && info.emittedVertices == 3 && info.primitives == 1
            && info.grouping == 1 && info.plan.field20 == 3 && info.plan.field18 == 3
            && mesh.HasPreparedPacketForAnalysis() && !mesh.HasExternalPacketForAnalysis()
            && mesh.GetField14ForAnalysis() == 3, "completed packet counters/state");
        auto clone = mesh.Clone();
        const auto* copy = dynamic_cast<spPS2MeshData*>(clone.get());
        Require(copy && !copy->HasPreparedPacketForAnalysis() && copy->PacketBytesForAnalysis().empty()
            && copy->GetField14ForAnalysis() == 4, "RTTI clone still has fresh empty packet state");
    }
    void InterleavedGolden()
    {
        spIndexBuffer indices; Triangle(indices);
        spVertexBuffer vertices; Vertices(vertices, true);
        spPS2MeshData mesh;
        const auto result = mesh.BuildPacketForAnalysis(indices, vertices, 0);
        Require(result.completed && result.nativeReturn && result.status == Status::Built,
            "interleaved raw-normal path complete");
        const auto expected = Bytes({
            0x6000000E, 0, 0x11000000, 0x11000000,
            0, 0, 0, 0x6C0C8000,
            14, 15, 0x3F800000, 0, 0xEE, 0xDD, 0xCC, 0xFF,
            0x3E800000, 0xBE800000, 0x00000001, 0, 7, 8, 9, 0,
            10, 11, 0x3F800000, 0, 0xB2, 0xC3, 0xD4, 0xA1,
            0x80000000, 0x7FC12345, 0x7F800000, 0, 1, 2, 3, 0,
            12, 13, 0x3F800000, 0, 0x20, 0x30, 0x40, 0x10,
            0x3F800000, 0, 0xBF800000, 0, 4, 5, 6, 0,
            0, 0, 0x04000003, 0x17000000});
        Require(mesh.PacketBytesForAnalysis() == expected,
            "whole interleaved packet raw normal/position and integer RGBA with UV=(u,v,1,0)");
        if (exportFixtures) actualPacketFixtures[1] = mesh.PacketBytesForAnalysis();
        Require(mesh.BuildPacketForAnalysis(indices, vertices, 99).completed
            && mesh.PacketBytesForAnalysis() == expected && mesh.GetFieldF8ForAnalysis() == 99,
            "all non-one grouping values select same interleaved branch");
    }
    void ChunksAndCoverage()
    {
        spVertexBuffer vertices;
        Require(vertices.InitializeFromDataForAnalysis(0x900, 1, 0, Bytes({1, 2, 3, 0xA1B2C3D4, 4, 5})),
            "repeated vertex");
        spIndexBuffer indices;
        Require(indices.InitializeForAnalysis(14, Type::Type2), "42-index chunk case");
        spPS2MeshData mesh;
        Require(mesh.BuildPacketForAnalysis(indices, vertices).completed, "39+3 grouped chunks built");
        const auto& bytes = mesh.PacketBytesForAnalysis();
        Require(bytes.size() == 1344 && Word(bytes, 0) == 0x60000053
            && Word(bytes, 6) == 0x01270103 && Word(bytes, 293) == 0x04000027
            && Word(bytes, 298) == 0x01030103 && Word(bytes, 333) == 0x04000003,
            "capacity39 boundary and tail3 headers/trailers exact");
        Require(mesh.PacketInfoForAnalysisView().qwords == 84
            && mesh.PacketInfoForAnalysisView().emittedVertices == 42
            && mesh.PacketInfoForAnalysisView().primitives == 14, "multi-chunk counters exact");
        Require(mesh.BuildPacketForAnalysis(indices, vertices, 2).completed,
            "interleaved multi-chunk built");
        Require(mesh.PacketBytesForAnalysis().size() == 2096
            && Word(mesh.PacketBytesForAnalysis(), 0) == 0x60000082
            && Word(mesh.PacketBytesForAnalysis(), 7) == 0x6C758000
            && Word(mesh.PacketBytesForAnalysis(), 478) == 0x04000027
            && Word(mesh.PacketBytesForAnalysis(), 483) == 0x6C098000,
            "interleaved capacity39 and tail3 exact");

        // Cover every admitted stream combination and both grouping branches.
        // Count expectations derive from VIF payload widths, not serializer code.
        spIndexBuffer triangle;
        Require(triangle.InitializeForAnalysis(1, Type::Type2), "all-stream triangle");
        for (unsigned uvCount = 1; uvCount <= 8; ++uvCount)
            for (unsigned hasNormal = 0; hasNormal < 2; ++hasNormal)
                for (unsigned hasColor = 0; hasColor < 2; ++hasColor)
                {
                    if (!hasNormal && !hasColor) continue;
                    const auto flags = (((1U << uvCount) - 1U) * 0x800U)
                        | (hasNormal ? 0x40U : 0U) | (hasColor ? 0x100U : 0U);
                    spVertexBuffer streams;
                    Require(streams.InitializeForAnalysis(flags, 1), "stream combination initialized");
                    Require(mesh.BuildPacketForAnalysis(triangle, streams, 0).completed,
                        "all supported interleaved stream combinations complete");
                    Require(mesh.PacketInfoForAnalysisView().qwords == 3 + 3 * (1 + uvCount + hasNormal + hasColor),
                        "interleaved stream count exact");
                    if (!hasNormal)
                    {
                        Require(mesh.BuildPacketForAnalysis(triangle, streams).completed,
                            "all supported grouped UV counts complete");
                        Require(mesh.PacketInfoForAnalysisView().qwords == 8 + uvCount * 3,
                            "grouped UV/alignment stream count exact");
                    }
                }
    }
    void Boundaries()
    {
        spIndexBuffer indices; Triangle(indices);
        spVertexBuffer vertices; Vertices(vertices, false);
        spPS2MeshData mesh;
        Require(mesh.BuildPacketForAnalysis(indices, vertices).completed, "baseline packet built");
        const auto saved = mesh.PacketBytesForAnalysis();
        const auto checkIncomplete = [&](const spIndexBuffer& input, const spVertexBuffer& vertex,
            std::uint32_t grouping, Status expected, std::uint32_t budget = 64U * 1024U * 1024U)
        {
            const auto result = mesh.BuildPacketForAnalysis(input, vertex, grouping, budget);
            Require(!result.completed && !result.nativeReturn && result.status == expected
                && mesh.PacketBytesForAnalysis() == saved && mesh.GetFieldF8ForAnalysis() == 1,
                "incomplete/host-guard result preserves prior packet state");
        };
        checkIncomplete(indices, vertices, 1, Status::ByteBudgetExceeded, 175);
        Require(indices.SetIndexForAnalysis(0, 3), "invalid vertex reference installed");
        checkIncomplete(indices, vertices, 1, Status::InvalidInput);
        Require(indices.SetIndexForAnalysis(0, 2), "valid reference restored");
        spIndexBuffer other;
        Require(other.InitializeForAnalysis(1, Type::Type3), "type3 initialized");
        const auto stripRejected = mesh.BuildPacketForAnalysis(other, vertices);
        Require(stripRejected.completed && !stripRejected.nativeReturn
            && stripRejected.status == Status::NativePreparationRejected && mesh.PacketBytesForAnalysis() == saved,
            "canonical Type3 stripifier returns false without inventing packet");
        Require(other.InitializeForAnalysis(1, Type::Type2, 1), "32bit indices initialized");
        checkIncomplete(other, vertices, 1, Status::InvalidInput);
        spVertexBuffer weights;
        Require(weights.InitializeForAnalysis(0x902, 3), "weights initialized");
        checkIncomplete(indices, weights, 1, Status::UnsupportedWeights);
        spVertexBuffer extra;
        Require(extra.InitializeForAnalysis(0x901, 3), "unsupported component initialized");
        checkIncomplete(indices, extra, 1, Status::UnsupportedComponents);
        spVertexBuffer hole;
        Require(hole.InitializeForAnalysis(0x2900, 3), "noncontiguous UV initialized");
        checkIncomplete(indices, hole, 1, Status::UnsupportedComponents);
        spVertexBuffer normal; Vertices(normal, true);
        checkIncomplete(indices, normal, 1, Status::UnsupportedCompressedNormal);
        spVertexBuffer absent;
        checkIncomplete(indices, absent, 1, Status::InvalidInput);
        Require(other.InitializeForAnalysis(1, Type::Type4), "type4 initialized");
        auto result = mesh.BuildPacketForAnalysis(other, absent);
        Require(result.completed && result.nativeReturn && result.status == Status::AlreadyPreparedInput
            && mesh.PacketBytesForAnalysis() == saved, "type4 immediate success invents no packet");
        spPS2MeshData empty;
        Require(empty.BuildPacketForAnalysis(other, absent).nativeReturn
            && !empty.HasPreparedPacketForAnalysis() && empty.PacketBytesForAnalysis().empty(),
            "type4 does not claim new packet presence");
        Require(other.InitializeForAnalysis(1, Type::Type1), "type1 initialized");
        result = mesh.BuildPacketForAnalysis(other, vertices);
        Require(result.completed && !result.nativeReturn && result.status == Status::RejectedNativeType,
            "unsupported native type has known false return");
        Require(other.InitializeForAnalysis(0, Type::Type2), "empty list initialized");
        result = mesh.BuildPacketForAnalysis(other, vertices);
        Require(result.completed && !result.nativeReturn && result.status == Status::EmptyIndexStream
            && mesh.PacketBytesForAnalysis() == saved, "empty list has known false return");
    }
    void WeightedPackets()
    {
        using sparkplug::evidence::ps2::PackMeshWeightsForAnalysis;
        const std::array<std::uint32_t, 4> raw{{0x80000000, 0x7FC12345, 0x3F800000, 0x00000001}};
        Require(!PackMeshWeightsForAnalysis(raw, 0xFE7F0201, 0)
            && !PackMeshWeightsForAnalysis(raw, 0xFE7F0201, 5), "weight record input domain explicit");
        const auto full = PackMeshWeightsForAnalysis(raw, 0xFE7F0201, 4);
        Require(full && *full == std::array<std::uint32_t, 8>{{
            0x80000000, 0x7FC12345, 0x3F800000, 0x00000001, 4, 8, 508, 1016}},
            "four raw weights preserved; byte bone indices shifted by two");
        spVertexBuffer vertices;
        Require(vertices.InitializeFromDataForAnalysis(0x922, 1, 0,
            Bytes({1, 2, 3, 0x80000000, 0xFE7F0201, 0xA1B2C3D4, 4, 5})), "compatible one-weight buffer");
        spIndexBuffer indices;
        Require(indices.InitializeForAnalysis(1, Type::Type2), "weighted triangle initialized");
        spPS2MeshData mesh;
        Require(mesh.BuildPacketForAnalysis(indices, vertices).completed, "weighted grouped triangle complete");
        const auto grouped = Bytes({
            0x60000011, 0, 0x11000000, 0x11000000,
            0, 0, 0x01030103, 0x64038000,
            4, 5, 4, 5, 4, 5, 0, 0,
            0, 0, 0x01030103, 0x6E03C001,
            0xA1D4C3B2, 0xA1D4C3B2, 0xA1D4C3B2, 0,
            0, 0, 0x01030103, 0x6C038002,
            1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0,
            0, 0, 0x01010101, 0x6C068009,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0, 0x04000003, 0x01010101, 0x17000000});
        Require(mesh.PacketBytesForAnalysis() == grouped && mesh.PacketInfoForAnalysisView().plan.field20 == 5
            && mesh.PacketInfoForAnalysisView().plan.field3C == 1, "whole weighted grouped literal packet");
        if (exportFixtures) actualPacketFixtures[2] = mesh.PacketBytesForAnalysis();
        Require(mesh.BuildPacketForAnalysis(indices, vertices, 0).completed, "weighted interleaved complete");
        const auto interleaved = Bytes({
            0x60000012, 0, 0x11000000, 0x11000000,
            0, 0, 0, 0x6C0F8000,
            4, 5, 0x3F800000, 0, 0xB2, 0xC3, 0xD4, 0xA1, 1, 2, 3, 0,
            4, 5, 0x3F800000, 0, 0xB2, 0xC3, 0xD4, 0xA1, 1, 2, 3, 0,
            4, 5, 0x3F800000, 0, 0xB2, 0xC3, 0xD4, 0xA1, 1, 2, 3, 0,
            0, 0, 0x01010101, 0x6C06800F,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0x80000000, 0, 0, 0, 4, 0, 0, 0,
            0, 0, 0x04000003, 0x17000000});
        Require(mesh.PacketBytesForAnalysis() == interleaved, "whole weighted interleaved literal packet");
        if (exportFixtures) actualPacketFixtures[3] = mesh.PacketBytesForAnalysis();
        Require(indices.InitializeForAnalysis(8, Type::Type2), "weighted chunk21+3 initialized");
        Require(mesh.BuildPacketForAnalysis(indices, vertices).completed
            && Word(mesh.PacketBytesForAnalysis(), 6) == 0x01150103,
            "weighted234 memory reservation gives capacity21 instead of unweighted39");
        for (unsigned weights = 1; weights <= 4; ++weights)
        {
            spVertexBuffer stream;
            const auto flags = 0x920U | (((1U << weights) - 1U) << 1);
            Require(stream.InitializeForAnalysis(flags, 1), "contiguous weighted layout initialized");
            Require(mesh.BuildPacketForAnalysis(indices, stream).completed
                && mesh.BuildPacketForAnalysis(indices, stream, 2).completed,
                "all one-to-four weight supported records build both grouping branches");
        }
        const auto saved = mesh.PacketBytesForAnalysis();
        spVertexBuffer needsExpansion;
        Require(needsExpansion.InitializeForAnalysis(0x22, 1), "weighted normalization input initialized");
        auto result = mesh.BuildPacketForAnalysis(indices, needsExpansion);
        Require(!result.completed && result.status == Status::UnsupportedWeightedNormalization
            && mesh.PacketBytesForAnalysis() == saved, "unknown normalized weight bytes stay incomplete");
        spVertexBuffer hole;
        Require(hole.InitializeForAnalysis(0x92A, 1), "weight hole initialized");
        result = mesh.BuildPacketForAnalysis(indices, hole);
        Require(!result.completed && result.status == Status::UnsupportedWeights,
            "noncontiguous weights do not read absent offsets as successful data");
    }
    void ExpandedWeightedNormalization()
    {
        for (unsigned fixture = 0; fixture < 4; ++fixture)
        {
            const std::uint32_t flags = fixture == 0 ? 0x22U : fixture == 1 ? 0x822U : fixture == 2 ? 0x62U : 0x122U;
            const auto source = fixture == 0 ? Bytes({
                1, 2, 3, 0xAAAAAAAA, 0xBBBBBBBB, 4, 5, 6, 0xCCCCCCCC, 0xDDDDDDDD}) : fixture == 1 ? Bytes({
                1, 2, 3, 0xAAAAAAAA, 0xBBBBBBBB, 10, 11,
                4, 5, 6, 0xCCCCCCCC, 0xDDDDDDDD, 12, 13}) : fixture == 2 ? Bytes({
                1, 2, 3, 0xAAAAAAAA, 0xBBBBBBBB, 0x80000000, 0x7FC12345, 0x3F800000,
                4, 5, 6, 0xCCCCCCCC, 0xDDDDDDDD, 0x3F000000, 0xBF000000, 0x00000001}) : Bytes({
                1, 2, 3, 0xAAAAAAAA, 0xBBBBBBBB, 0xA1B2C3D4,
                4, 5, 6, 0xCCCCCCCC, 0xDDDDDDDD, 0x10203040});
            const auto allocation = fixture != 2 ? Bytes({
                77, 77, 77, 0x3F800000, 0xFE7F0201, 77, 77, 77,
                88, 88, 88, 0x00000001, 0x01020304, 88, 88, 88}) : Bytes({
                77, 77, 77, 0x3F800000, 0xFE7F0201, 77, 77, 77, 77, 77,
                88, 88, 88, 0x00000001, 0x01020304, 88, 88, 88, 88, 88});
            const auto expected = fixture == 0 ? Bytes({
                1, 2, 3, 0x3F800000, 0xFE7F0201, 0, 0, 0,
                4, 5, 6, 0x00000001, 0x01020304, 0, 0, 0}) : fixture == 1 ? Bytes({
                1, 2, 3, 0x3F800000, 0xFE7F0201, 0, 10, 11,
                4, 5, 6, 0x00000001, 0x01020304, 0, 12, 13}) : fixture == 2 ? Bytes({
                0, 2, 3, 0x3F800000, 0xFE7F0201, 0x80000000, 0x7FC12345, 0x3F800000, 0, 0,
                0, 5, 6, 0x00000001, 0x01020304, 0x3F000000, 0xBF000000, 0x00000001, 0, 0}) : Bytes({
                1, 2, 3, 0x3F800000, 0xFE7F0201, 0xA1B2C3D4, 0, 0,
                4, 5, 6, 0x00000001, 0x01020304, 0x10203040, 0, 0});
            spVertexBuffer vertices;
            Require(vertices.InitializeFromDataForAnalysis(flags, 2, 0, source), "expanded weighted source initialized");
            auto normalized = spPS2MeshData::NormalizeVertexBufferForAnalysis(vertices, &allocation);
            Require(normalized.completed && normalized.capturedAllocationUsed
                && normalized.buffer->GetDataForAnalysis() == expected,
                "expanded weights/bones retain captured allocation, never source or implicit host zero");
            Require(std::count(normalized.writtenBytes.begin(), normalized.writtenBytes.end(), std::uint8_t{0}) == 16,
                "two vertices retain exactly eight allocation bytes per vertex");
            spIndexBuffer indices;
            Require(indices.InitializeForAnalysis(1, Type::Type2)
                && indices.SetIndexForAnalysis(0, 1) && indices.SetIndexForAnalysis(1, 0)
                && indices.SetIndexForAnalysis(2, 1), "expanded weighted order1,0,1");
            const auto grouping = fixture == 2 ? 0U : 1U;
            spPS2MeshData expanded, alreadyNormalized;
            const auto withoutCapture = expanded.BuildPacketForAnalysis(indices, vertices, grouping);
            Require(!withoutCapture.completed && withoutCapture.status == Status::UnsupportedWeightedNormalization,
                "expanded weighted default remains explicit rejection without allocation bytes");
            Require(expanded.BuildPacketForAnalysis(indices, vertices, grouping, 4096, &allocation).completed
                && alreadyNormalized.BuildPacketForAnalysis(indices, *normalized.buffer, grouping).completed
                && expanded.PacketBytesForAnalysis() == alreadyNormalized.PacketBytesForAnalysis(),
                "whole weighted packet uses captured normalized data through same packer");
            Require(vertices.GetDataForAnalysis() == source && normalized.buffer->GetDataForAnalysis() == expected,
                "own packet lifetime does not mutate borrowed inputs or normalized buffer");
            const auto saved = expanded.PacketBytesForAnalysis();
            const std::vector<std::byte> wrongSize(allocation.size() - 1);
            const auto malformed = expanded.BuildPacketForAnalysis(indices, vertices, grouping, 4096, &wrongSize);
            Require(!malformed.completed && malformed.status == Status::NormalizationIncomplete
                && expanded.PacketBytesForAnalysis() == saved, "wrong captured allocation size preserves previous packet");
            if (exportExpandedFixtures)
            {
                actualExpandedFixtures[fixture] = {flags, normalized.buffer->GetComponentFlagsForAnalysis(),
                    static_cast<std::uint32_t>(normalized.buffer->GetVertexStrideForAnalysis() / 4), grouping,
                    source, allocation, normalized.buffer->GetDataForAnalysis(), expanded.PacketBytesForAnalysis(), normalized.writtenBytes};
            }
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--packet-fixtures") exportFixtures = true;
        else if (argc == 2 && std::string(argv[1]) == "--expanded-weighted-fixtures") exportExpandedFixtures = true;
        else if (argc != 1) throw std::runtime_error("expected no arguments or --packet-fixtures");
        Normalization(); GroupedGolden(); InterleavedGolden(); ChunksAndCoverage(); Boundaries(); WeightedPackets(); ExpandedWeightedNormalization();
        if (exportFixtures)
        {
            constexpr std::array<const char*, 4> names{{"grouped", "interleaved", "weighted-grouped", "weighted-interleaved"}};
            std::printf("{\"checks\":%u,\"packets\":{", checks);
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                std::printf("%s\"%s\":\"", index ? "," : "", names[index]);
                PrintHex(actualPacketFixtures[index]);
                std::printf("\"");
            }
            std::printf("}}\n");
        }
        else if (exportExpandedFixtures)
        {
            std::printf("{\"checks\":%u,\"fixtures\":[", checks);
            for (std::size_t i = 0; i < actualExpandedFixtures.size(); ++i)
            {
                const auto& f = actualExpandedFixtures[i];
                std::printf("%s{\"sourceFlags\":%u,\"effectiveFlags\":%u,\"strideWords\":%u,\"grouping\":%u,\"count\":2,\"source\":\"",
                    i ? "," : "", f.sourceFlags, f.effectiveFlags, f.strideWords, f.grouping);
                PrintHex(f.source); std::printf("\",\"allocation\":\""); PrintHex(f.allocation);
                std::printf("\",\"normalized\":\""); PrintHex(f.normalized);
                std::printf("\",\"packet\":\""); PrintHex(f.packet);
                std::printf("\",\"written\":\"");
                for (auto value : f.written) std::printf("%02X", static_cast<unsigned>(value));
                std::printf("\"}");
            }
            std::printf("]}\n");
        }
        else std::printf("PASS %u/%u: PS2 normalization writes and whole supported packets\n", checks, checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
