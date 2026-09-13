#include "Analysis/PC/spBallisticCpuDraw.h"

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
    int Probe(bool stored = false, bool withBasis = false)
    {
        unsigned count = 0;
        if (!(std::cin >> count) || count > 4096) return 2;
        std::cout << std::setprecision(17) << '[';
        for (unsigned sample = 0; sample < count; ++sample)
        {
            BallisticCpuParticleRecord record;
            BallisticCpuParametersForAnalysis parameters;
            unsigned active = 0, vertices = 0, indices = 0;
            for (auto& value : record) std::cin >> value;
            std::cin >> parameters.currentTime;
            for (auto& value : parameters.accelerationBegin) std::cin >> value;
            for (auto& value : parameters.accelerationEnd) std::cin >> value;
            for (auto& value : parameters.runtimeScales) std::cin >> value;
            // Retain the evidence protocol's two source ARGB inputs; packed
            // color is outside the qualified motion reconstruction.
            std::array<std::uint32_t,2> originalColors{};
            for (auto& value : originalColors) std::cin >> value;
            std::cin >> active >> vertices >> indices;
            std::array<float,9> camera{};
            std::array<float,3> scale{};
            std::array<float,3> nodePosition{};
            unsigned worldSpace=0;
            if (withBasis)
            {
                for (auto& value : camera) std::cin >> value;
                for (auto& value : scale) std::cin >> value;
                for (auto& value : nodePosition) std::cin >> value;
                std::cin >> worldSpace;
            }
            if (!std::cin) return 2;
            BallisticCpuSampleForAnalysis result;
            std::vector<BallisticCpuBatchForAnalysis> batches;
            const bool accepted = stored
                ? EvaluateBallisticCpuStoredPositionForAnalysis(record,parameters.currentTime,parameters.runtimeScales,result)
                : EvaluateBallisticCpuMotionForAnalysis(record, parameters, result);
            const bool planned = PlanBallisticCpuBatchesForAnalysis(active, vertices, indices, batches);
            if (sample) std::cout << ',';
            std::cout << "{\"accepted\":" << (accepted ? "true" : "false")
                << ",\"planned\":" << (planned ? "true" : "false");
            if (accepted)
                std::cout << ",\"age\":" << result.age << ",\"normalizedAge\":" << result.normalizedAge
                    << ",\"center\":[" << result.center[0] << ',' << result.center[1] << ',' << result.center[2]
                    << "],\"halfSize\":" << result.halfSize;
            std::cout << ",\"batches\":[";
            for (unsigned i = 0; i < batches.size(); ++i)
                std::cout << (i ? "," : "") << '[' << batches[i].firstParticle << ',' << batches[i].particleCount << ']';
            std::cout << ']';
            if (withBasis)
            {
                BallisticCpuBasisForAnalysis basis;
                BallisticCpuQuadPositionsForAnalysis quad;
                std::array<float,16> worldMatrix{};
                const bool built = BuildBallisticCpuBasisForAnalysis(camera,scale,basis)
                    && accepted && BuildBallisticCpuQuadPositionsForAnalysis(result,basis,quad)
                    && BuildBallisticCpuWorldMatrixForAnalysis(nodePosition,worldSpace!=0,worldMatrix);
                std::cout << ",\"basisAccepted\":" << (built ? "true" : "false");
                if (built)
                {
                    std::cout << ",\"basis\":[";
                    bool first=true;
                    for (const auto& row : {basis.right,basis.up,basis.forward})
                    { std::cout << (first ? "" : ",") << '[' << row[0] << ',' << row[1] << ',' << row[2] << ']';first=false; }
                    std::cout << "],\"quad\":[";first=true;
                    for (const auto& row : quad)
                    { std::cout << (first ? "" : ",") << '[' << row[0] << ',' << row[1] << ',' << row[2] << ']';first=false; }
                    std::cout << "],\"worldMatrix\":[";first=true;
                    for (const auto value : worldMatrix)
                    { std::cout << (first ? "" : ",") << value;first=false; }
                    std::cout << ']';
                }
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
    if (argc == 2 && std::string_view(argv[1]) == "--probe-stored") return Probe(true,true);
    if (argc == 2 && std::string_view(argv[1]) == "--probe-quad") return Probe(false,true);
    try
    {
        BallisticCpuParticleRecord record{1,2,3,.5f,-.25f,.75f,0,2};
        BallisticCpuParametersForAnalysis p{1,{.25f,.5f,1},{.25f,.5f,1},{.5f,1}};
        BallisticCpuSampleForAnalysis result;
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result), "ordinary sample");
        Check(result.center == std::array<float,3>{1.625f,2,4.25f}, "constant acceleration center");
        Check(result.halfSize == .375f, "runtime scale interpolation");
        p.currentTime = 2;
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result) && result.age == 2, "exact endpoint stays at lifetime");
        p.currentTime = 4;
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result) && result.age == 0, "positive multiple wraps by quotient");
        p.currentTime = -3.25f;
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result) && result.age == 1.75, "negative correction truncates age before division");
        p.currentTime = -1.75f; record[7] = .5f;
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result) && result.age == -.25, "negative correction need not reach nonnegative age");
        record[7] = 2; p.currentTime = 1; p.accelerationEnd = {-.25f,1,.5f};
        Check(EvaluateBallisticCpuMotionForAnalysis(record,p,result) && std::abs(result.center[0]-1.583333333f)<1e-6f,
            "cubic term uses acceleration end minus begin");
        const auto saved = result;
        record[7] = 0;
        Check(!EvaluateBallisticCpuMotionForAnalysis(record,p,result), "zero lifetime is outside host boundary");
        record[7] = 2; record[0] = std::numeric_limits<float>::quiet_NaN();
        Check(!EvaluateBallisticCpuMotionForAnalysis(record,p,result), "nonfinite input refused");
        Check(result.center==saved.center && result.halfSize==saved.halfSize, "refusals preserve output");
        std::vector<BallisticCpuBatchForAnalysis> batches;
        Check(PlanBallisticCpuBatchesForAnalysis(8,32,96,batches) && batches.size()==1 && batches[0].particleCount==0,
            "exact capacity keeps original empty final batch");
        Check(PlanBallisticCpuBatchesForAnalysis(16,32,96,batches) && batches.size()==2
            && batches[0].particleCount==8 && batches[1].firstParticle==8 && batches[1].particleCount==0, "two exact capacities");
        Check(PlanBallisticCpuBatchesForAnalysis(9,32,96,batches) && batches.size()==2 && batches[1].particleCount==1, "nonzero remainder");
        Check(PlanBallisticCpuBatchesForAnalysis(5,32,24,batches) && batches[0].particleCount==4, "index count also bounds the batch");
        Check(PlanBallisticCpuBatchesForAnalysis(0,32,96,batches) && batches.empty(), "no active particles means no batches");
        Check(!PlanBallisticCpuBatchesForAnalysis(1,3,96,batches) && batches.empty(), "native divide by zero not fabricated");
        Check(!PlanBallisticCpuBatchesForAnalysis(65537,32,96,batches), "host allocation bound");
        record={1,2,3,128,-512,1024,0,2};
        Check(EvaluateBallisticCpuStoredPositionForAnalysis(record,5,{.5f,1},result)
            && result.age==5 && result.normalizedAge==2.5 && result.halfSize==.875f
            && result.center==std::array<float,3>{1,2,3}, "stored-position branch neither integrates velocity nor wraps age");
        Check(EvaluateBallisticCpuStoredPositionForAnalysis(record,-3.25f,{.5f,1},result)
            && result.age==-3.25 && result.halfSize==-.15625f, "negative stored age extrapolates signed size");
        const auto storedSaved=result;
        record[7]=0;
        Check(!EvaluateBallisticCpuStoredPositionForAnalysis(record,1,{1,1},result)
            && result.center==storedSaved.center, "stored host lifetime guard preserves output");
        const std::array<float,9> identity{1,0,0,0,1,0,0,0,1};
        BallisticCpuBasisForAnalysis basis;
        Check(BuildBallisticCpuBasisForAnalysis(identity,{1,1,1},basis)
            && basis.right==std::array<float,3>{-1,0,0} && basis.up==std::array<float,3>{0,1,0}, "original quad handedness");
        result.center={1,2,3};result.halfSize=.375f;
        BallisticCpuQuadPositionsForAnalysis quad;
        Check(BuildBallisticCpuQuadPositionsForAnalysis(result,basis,quad)
            && quad[0]==std::array<float,3>{.625f,1.625f,3}
            && quad[2]==std::array<float,3>{1.375f,2.375f,3}, "observed original corner order");
        const std::array<float,9> pole{1,0,0,0,0,1,0,1,0};
        Check(BuildBallisticCpuBasisForAnalysis(pole,{1,1,1},basis)
            && basis.right==std::array<float,3>{1,0,0} && basis.up==std::array<float,3>{0,0,1}, "parallel case takes camera second row");
        Check(BuildBallisticCpuBasisForAnalysis(pole,{1,2,1},basis)
            && basis.right==std::array<float,3>{0,0,0} && basis.forward[1]==-2, "scaled forward is not normalized for parallel test");
        Check(BuildBallisticCpuBasisForAnalysis({}, {1,1,1},basis)
            && basis.right==std::array<float,3>{0,0,0}, "short original basis collapses to zero");
        const auto basisSaved=basis;
        Check(!BuildBallisticCpuBasisForAnalysis(identity,{1,std::numeric_limits<float>::infinity(),1},basis)
            && basis.forward==basisSaved.forward, "nonfinite basis guard preserves output");
        auto hugeCamera=identity;hugeCamera[8]=1e30f;
        Check(!BuildBallisticCpuBasisForAnalysis(hugeCamera,{1,1,1},basis)
            && basis.forward==basisSaved.forward, "host math overflow is refused, not a fabricated collapsed quad");
        std::cout << "PASS Ballistic CPU draw: " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
