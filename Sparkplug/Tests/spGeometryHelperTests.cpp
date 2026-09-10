#include "Analysis/PC/GeometryHelper4604F0.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
using Helper = sparkplug::evidence::pc::GeometryHelper4604F0;
namespace
{
int checks = 0;
void Check(bool result, const char* label)
{
    ++checks;
    if (!result) throw std::runtime_error(label);
}
using Point = std::array<float, 3>;
const std::vector<Point> Square{{-10,-10,10},{10,-10,10},{10,10,10},{-10,10,10}};

void Fill(spIndexBuffer& ib, spVertexBuffer& vb,
    const std::vector<Point>& positions, const std::vector<std::uint16_t>& indices,
    std::uint32_t indexFlags = 0)
{
    Check(ib.InitializeForAnalysis(static_cast<std::uint32_t>(indices.size() / 3),
        spIndexBuffer::eIndexBufferType::Type2, indexFlags), "initialize fixture IB");
    for (std::uint32_t i = 0; i < indices.size(); ++i)
        Check(ib.SetIndexForAnalysis(i, indices[i]), "fixture index");
    std::vector<std::byte> bytes(positions.size() * 12);
    for (std::size_t i = 0; i < positions.size(); ++i)
        std::memcpy(bytes.data() + i * 12, positions[i].data(), 12);
    Check(vb.InitializeFromDataForAnalysis(0, static_cast<std::uint32_t>(positions.size()),
        0, bytes), "initialize fixture VB");
}

struct CapturedSort
{
    std::vector<std::uint16_t> ids;
    std::size_t calls = 0;
    static bool Invoke(void* opaque, std::uint16_t* ids, std::size_t count,
        std::size_t width, Helper::ComparatorForAnalysis compare, const void* context)
    {
        auto& fixture = *static_cast<CapturedSort*>(opaque);
        ++fixture.calls;
        Check(width == 2 && count == fixture.ids.size(), "actual qsort count and UInt16 width");
        Check(compare && context, "original comparator has explicit buffer context");
        for (std::size_t i = 0; i < count; ++i)
            Check(ids[i] == i, "460C90 supplies original increasing vertex IDs");
        // Replay ONLY captured dependency output. No fixture sorting algorithm
        // and no assumption that another MSVCR71 version picks the same ties.
        std::copy(fixture.ids.begin(), fixture.ids.end(), ids);
        return true;
    }
    Helper::SortDispatchForAnalysis Dispatch() { return {this, &Invoke}; }
};

void ActualCase(const std::vector<Point>& positions, const std::vector<std::uint16_t>& input,
    const std::vector<std::uint16_t>& sorted, const std::vector<std::uint16_t>& expectedIndices,
    const std::vector<Point>& expectedPositions, bool duplicates, std::uint32_t allocation)
{
    spIndexBuffer ib; spVertexBuffer vb; Fill(ib, vb, positions, input);
    CapturedSort fixture{sorted}; Helper helper; Helper::ObservationForAnalysis observation;
    std::string error;
    Check(helper.WeldForAnalysis(ib, vb, fixture.Dispatch(), observation, &error),
        "shared original caller completes with declared captured qsort output");
    Check(error.empty() && fixture.calls == 1, "sort is called once with no hidden fallback");
    Check(observation.sortedIds == sorted, "captured 460CAF IDs are retained before folding");
    Check(observation.duplicateFound == duplicates && observation.compactionCalled == duplicates,
        "460D25 only invokes compactor when equal keys were found");
    Check(observation.compactionAllocationBytes == allocation, "actual oversized compactor allocation request");
    Check(ib.GetIndexCountForAnalysis() == expectedIndices.size(), "actual IB count retained");
    for (std::uint32_t i = 0; i < expectedIndices.size(); ++i)
        Check(ib.GetIndexForAnalysis(i) == expectedIndices[i], "actual final UInt16 IB word");
    Check(vb.GetVertexCountForAnalysis() == expectedPositions.size(), "actual compacted VB count");
    Check(vb.GetDataForAnalysis().size() == expectedPositions.size() * 12,
        "logical VB bytes exclude the unused oversized allocation tail");
    for (std::size_t i = 0; i < expectedPositions.size(); ++i)
        Check(std::memcmp(vb.GetDataForAnalysis().data() + i * 12,
            expectedPositions[i].data(), 12) == 0, "actual final raw vertex record order");
}

void OriginalOutputs()
{
    // local-data/.../occlusion-optimizer/legacy-qsort-weld-run1.json:
    // original EXE 460D90 + MSVCR71 7.10.7031.4 qsort/shortsort instructions.
    // DLL SHA DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46.
    ActualCase(Square, {0,1,2,0,2,3}, {2,1,3,0}, {0,1,2,0,2,3}, Square, false, 0);
    auto five = Square; five.push_back(Square[0]);
    const std::vector<Point> compacted{Square[1],Square[2],Square[3],Square[0]};
    ActualCase(five, {0,1,2,4,2,3}, {2,1,3,4,0}, {3,0,1,3,1,2}, compacted, true, 192);
    auto nine = Square; nine.push_back({20,0,10});
    nine.insert(nine.end(), Square.begin(), Square.end());
    ActualCase(nine, {0,1,2,5,7,8}, {2,7,1,6,3,8,5,0,4},
        {3,0,1,3,1,2}, compacted, true, 192);
    ActualCase(std::vector<Point>(12, Square[0]), {0,1,2,9,10,11},
        {0,1,2,3,4,5,6,7,8,9,10,11}, {0,0,0,0,0,0}, {Square[0]}, true, 48);

    // Historical explicit stable CRT fixture in probe_pc_occlusion_runtime.py
    // produced different valid representative IDs with the same original game
    // caller. This remains a fixture input, never a production sorter policy.
    ActualCase(five, {0,1,2,4,2,3}, {2,1,3,0,4}, {0,1,2,0,2,3}, Square, true, 192);
}

void ComparatorAndGuards()
{
    spIndexBuffer ib; spVertexBuffer vb; Fill(ib, vb, Square, {0,1,2,0,2,3});
    const auto saved = vb.GetDataForAnalysis();
    Check(Helper::ComparePositionsForAnalysis(vb,0,1) == 1,
        "actual 4607F0 puts negative ten after positive ten by unsigned raw bytes");
    Check(Helper::ComparePositionsForAnalysis(vb,1,0) == -1, "raw comparator reverse sign");
    Check(!Helper::ComparePositionsForAnalysis(vb,0,4), "host comparator index guard");
    Helper helper; Helper::ObservationForAnalysis observation; std::string error;
    Check(!helper.WeldForAnalysis(ib,vb,{},observation,&error) && !error.empty(),
        "missing sort dependency is explicitly unsupported");
    for (const auto& invalid : std::vector<std::vector<std::uint16_t>>{
            {2,1,3,3}, {2,1,3,4}, {0,1,2,3}})
    {
        CapturedSort fixture{invalid};
        Check(!helper.WeldForAnalysis(ib,vb,fixture.Dispatch(),observation,&error),
            "malformed permutation or wrong comparator order rejected before geometry mutation");
        Check(vb.GetDataForAnalysis() == saved && ib.GetIndexForAnalysis(0) == 0
            && ib.GetIndexForAnalysis(5) == 3, "callback refusal preserves source buffers");
    }
    spIndexBuffer wide; spVertexBuffer wideVb; Fill(wide,wideVb,Square,{0,1,2,0,2,3},1);
    CapturedSort good{{2,1,3,0}};
    Check(!helper.WeldForAnalysis(wide,wideVb,good.Dispatch(),observation,&error) && good.calls == 0,
        "UInt32 dispatcher branch remains unsupported without calling sort");
    Check(vb.InitializeForAnalysis(0x40,4), "explicit non-position fixture");
    Check(!helper.WeldForAnalysis(ib,vb,good.Dispatch(),observation,&error) && good.calls == 0,
        "unrestored optional comparator components remain unsupported");
    const std::vector<Point> zero{{0,0,0},{-0.0F,0,0}};
    spIndexBuffer zeroIb; spVertexBuffer zeroVb; Fill(zeroIb,zeroVb,zero,{0,1,0});
    Check(Helper::ComparePositionsForAnalysis(zeroVb,0,1) == -1,
        "actual original comparator keeps positive and negative zero distinct");
    spIndexBuffer emptyIb; spVertexBuffer emptyVb;
    Fill(emptyIb,emptyVb,{},{});
    Check(!helper.WeldForAnalysis(emptyIb,emptyVb,good.Dispatch(),observation,&error)
        && good.calls == 0, "empty standalone weld has no original proof and cannot invoke sort");
    Check(!helper.CompactForAnalysis(emptyIb,emptyVb,observation,&error),
        "empty standalone compactor remains explicitly unsupported");
    Check(!helper.WeldForAnalysis(emptyIb,zeroVb,good.Dispatch(),observation,&error),
        "nonempty vertices do not imply support for a zero-index call");
}
}

int main()
{
    try
    {
        OriginalOutputs(); ComparatorAndGuards();
        std::cout << "PASS " << checks << "/" << checks << ": original geometry helper with explicit sort dependency\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
