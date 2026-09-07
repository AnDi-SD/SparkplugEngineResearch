#include "Code/Sparkplug/spBSPNode.h"
#include "Analysis/PC/SparkplugAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    float FromBits(std::uint32_t word)
    {
        float value;
        std::memcpy(&value, &word, 4);
        return value;
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t word;
        std::memcpy(&word, &value, 4);
        return word;
    }
    float ReadFloat()
    {
        std::uint32_t word;
        if (!(std::cin >> word))
            throw std::runtime_error("truncated input");
        return FromBits(word);
    }
    template <std::size_t N> std::array<float, N> ReadFloats()
    {
        std::array<float, N> result;
        for (auto& value : result)
            value = ReadFloat();
        return result;
    }
    void Batch()
    {
        unsigned count;
        while (std::cin >> count)
        {
            if (count > 16)
                throw std::runtime_error("bounded polygon");
            spBSPNode node;
            node.SetPlaneForAnalysis(ReadFloats<4>());
            std::vector<spBSPNode::Vector3> polygon(count);
            for (auto& point : polygon)
                point = ReadFloats<3>();
            if (!node.SetPolygonForAnalysis(polygon))
                throw std::runtime_error("polygon setup");
            const auto point = ReadFloats<3>();
            const auto sphere = ReadFloats<4>();
            const auto camera = ReadFloats<3>();
            const auto ray = ReadFloats<6>();
            spBSPNode::PlaneSet planes;
            std::cin >> count >> planes.activeCount;
            if (count > 16)
                throw std::runtime_error("bounded planes");
            for (unsigned i = 0; i < count; ++i)
            {
                auto equation = ReadFloats<4>();
                unsigned enabled;
                std::cin >> enabled;
                planes.planes.push_back({equation, enabled != 0});
            }
            if (!node.SetChildForAnalysis(0, std::make_unique<spPartitionNode>()) ||
                !node.SetChildForAnalysis(1, std::make_unique<spPartitionNode>()))
                throw std::runtime_error("children");
            const unsigned side =
                node.FindLeafForAnalysis(point, false) == node.GetChildForAnalysis(0) ? 0 : 1;
            const auto candidates = node.RayCandidatesForAnalysis(ray);
            std::cout << side << ',' << unsigned(node.PointMaskForAnalysis(point)) << ','
                      << unsigned(node.SphereMaskForAnalysis(sphere)) << ','
                      << node.VisibleChildrenForAnalysis(planes, camera) << ','
                      << candidates.size();
            for (const auto& candidate : candidates)
                std::cout << ',' << candidate.child << ',' << Bits(candidate.parameter);
            std::cout << '\n';
        }
    }
    void Units()
    {
        spBSPNode node;
        Check(node.GetChildCountForAnalysis() == 2 && !node.HasPlaneForAnalysis() &&
                  node.GetPolygonVertexCount() == 0,
              "native initial two slots and explicit plane gap");
        Check(!node.FindLeafForAnalysis({}) && node.PointMaskForAnalysis({}) == 0,
              "host unknown/null guards");
        node.SetZonePresentForAnalysis(true);
        Check(node.FindLeafForAnalysis({}) == &node, "Zone stop before plane");
        node.SetZonePresentForAnalysis(false);
        node.SetPlaneForAnalysis({1, 0, 0, 0});
        Check(node.SetChildForAnalysis(0, std::make_unique<spPartitionNode>()) &&
                  node.SetChildForAnalysis(1, std::make_unique<spPartitionNode>()),
              "owned children");
        Check(node.FindLeafForAnalysis({1, 0, 0}) == node.GetChildForAnalysis(0) &&
                  node.FindLeafForAnalysis({0, 0, 0}) == node.GetChildForAnalysis(1),
              "positive0 zero1");
        Check(node.PointMaskForAnalysis({0, 0, 0}) == 3 &&
                  node.PointMaskForAnalysis({1, 0, 0}) == 1 &&
                  node.PointMaskForAnalysis({-1, 0, 0}) == 2,
              "point epsilon masks");
        Check(node.SphereMaskForAnalysis({1, 0, 0, 1}) == 3 &&
                  node.SphereMaskForAnalysis({-2, 0, 0, 1}) == 2,
              "tangent sphere both sides");
        Check(spBSPNode::TraversalOrderForAnalysis(0) == 8 &&
                  spBSPNode::TraversalOrderForAnalysis(1) == 1,
              "two native packed orders");
        spBSPNode::PlaneSet planes;
        Check(node.VisibleChildrenForAnalysis(planes, {1, 0, 0}) == 0x82 &&
                  node.VisibleChildrenForAnalysis(planes, {0, 0, 0}) == 0x12,
              "no planes both children camera-side order");
        planes.planes.push_back({{0, 0, 1, 0}, true});
        planes.activeCount = 1;
        Check(node.VisibleChildrenForAnalysis(planes, {1, 0, 0}) == 1,
              "empty polygon plus enabled plane gives one near-side child");
        Check(node.SetPolygonForAnalysis({{-1, -1, 10}, {1, -1, 10}, {1, 1, 10}, {-1, 1, 10}}) &&
                  node.GetPlane() == spBSPNode::Plane{1, 0, 0, 0},
              "optional polygon independent of plane");
        Check(node.VisibleChildrenForAnalysis(planes, {1, 0, 0}) == 0x82,
              "positive polygon reaches both children");
        planes.planes[0].equation = {0, 0, 1, 10};
        Check(node.VisibleChildrenForAnalysis(planes, {1, 0, 0}) == 1,
              "coplanar polygon is not strictly positive");
        node.ReducePlanesForChildForAnalysis(0, planes);
        Check(planes.activeCount == 1 && planes.planes[0].enabled,
              "BSP plane reduction slot is no-op");
        const auto ray = node.RayCandidatesForAnalysis({-2, 0, 0, 1, 0, 0});
        Check(ray.size() == 2 && ray[0].child == 1 && ray[1].child == 0 && ray[1].parameter == 2,
              "forward ray plane crossing");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        Check(node.FindLeafForAnalysis({nan, 0, 0}) == node.GetChildForAnalysis(1) &&
                  node.PointMaskForAnalysis({nan, 0, 0}) == 3,
              "unordered leaf1 versus mask3");
        auto clone = node.Clone();
        auto* typed = dynamic_cast<spBSPNode*>(clone.get());
        Check(typed && !typed->HasPlaneForAnalysis() && typed->GetChildCountForAnalysis() == 2 &&
                  !typed->GetChildForAnalysis(0) && typed->GetPolygonVertexCount() == 0,
              "Base-only clone drops own graph/geometry");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            Batch();
        else
        {
            Units();
            std::cout << "PASS " << checks << '/' << checks << ": BSP source\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
