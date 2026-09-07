#include "Code/Sparkplug/spOctreeNode.h"
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
    float FloatFromBits(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    float ReadFloat()
    {
        std::uint32_t bits;
        if (!(std::cin >> bits))
            throw std::runtime_error("incomplete float");
        return FloatFromBits(bits);
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
        std::uint32_t first;
        while (std::cin >> first)
        {
            spOctreeNode::Vector3 pivot{FloatFromBits(first), ReadFloat(), ReadFloat()};
            const auto mins = ReadFloats<3>(), maxs = ReadFloats<3>();
            const auto point = ReadFloats<3>();
            const auto sphere = ReadFloats<4>();
            const auto camera = ReadFloats<3>();
            unsigned count, child;
            spOctreeNode::PlaneSet planes;
            std::cin >> count >> planes.activeCount >> child;
            if (count > 16 || child >= 8)
                throw std::runtime_error("bounded plane fixture");
            for (unsigned i = 0; i < count; ++i)
            {
                sparkplug::evidence::pc::visibility_math::Plane plane;
                plane.equation = ReadFloats<4>();
                unsigned enabled;
                std::cin >> enabled;
                plane.enabled = enabled != 0;
                planes.planes.push_back(plane);
            }
            spOctreeNode node;
            if (!node.SetGeometryForAnalysis(pivot, mins, maxs))
                throw std::runtime_error("invalid geometry fixture");
            const auto octant = spOctreeNode::OctantForAnalysis(point, pivot);
            std::cout << octant << ',' << unsigned(node.PointMaskForAnalysis(point)) << ','
                      << unsigned(node.SphereMaskForAnalysis(sphere)) << ','
                      << spOctreeNode::TraversalOrderForAnalysis(octant) << ','
                      << node.VisibleChildrenForAnalysis(planes, camera);
            node.ReducePlanesForChildForAnalysis(child, planes);
            std::cout << ',' << planes.activeCount;
            for (const auto& plane : planes.planes)
                std::cout << ',' << plane.enabled;
            std::cout << '\n';
        }
    }
    void UnitTests()
    {
        spPartitionNode leaf;
        Check(leaf.GetChildCountForAnalysis() == 0 && leaf.FindLeafForAnalysis({}) == &leaf,
              "base zero children and identity query");
        spOctreeNode node;
        Check(node.GetChildCountForAnalysis() == 8 && !node.HasGeometryForAnalysis(),
              "eight null children, pivot not invented initialized");
        Check(!node.FindLeafForAnalysis({}), "unset geometry host guard");
        node.SetZonePresentForAnalysis(true);
        Check(node.FindLeafForAnalysis({}) == &node, "zone stop before geometry access");
        node.SetZonePresentForAnalysis(false);
        Check(node.SetGeometryForAnalysis({1, 2, 3}, {-5, -6, -7}, {10, 20, 30}),
              "nonmidpoint pivot");
        for (unsigned i = 0; i < 8; ++i)
            Check(node.SetChildForAnalysis(i, std::make_unique<spPartitionNode>()),
                  "direct-owned child");
        Check(node.FindLeafForAnalysis({1, 2, 3}) == node.GetChildForAnalysis(0), "pivot ties low");
        Check(node.FindLeafForAnalysis({2, 3, 4}) == node.GetChildForAnalysis(7),
              "strict high XYZ");
        Check(node.PointMaskForAnalysis({1, 2, 3}) == 255, "point epsilon all octants");
        Check(node.PointMaskForAnalysis({2, 3, 4}) == 128, "point unique octant");
        Check(!node.SetGeometryForAnalysis({100, 2, 3}, {-5, -6, -7}, {10, 20, 30}) &&
                  node.FindLeafForAnalysis({2, 3, 4}) == node.GetChildForAnalysis(7),
              "host guard preserves state");
        auto clone = node.Clone();
        auto* typed = dynamic_cast<spOctreeNode*>(clone.get());
        Check(typed && !typed->HasGeometryForAnalysis() && typed->GetChildCountForAnalysis() == 8 &&
                  !typed->GetChildForAnalysis(0),
              "original Base-only clone fresh eight slots");
        Check(node.IsKindOf(spPartitionNode::ClassID) && node.IsKindOf(spBaseObject::ClassID) &&
                  node.IsExactly(spOctreeNode::ClassID),
              "original RTTI chain");
        spOctreeNode zero;
        Check(zero.SetGeometryForAnalysis({0, 0, 0}, {-10, -10, -10}, {10, 10, 10}),
              "centered fixture");
        Check(zero.SphereMaskForAnalysis({.7F, .7F, .9F, 1}) == 0xF0,
              "native axis-line shortcut intentionally differs from generic sphere octant overlap");
        Check(zero.SphereMaskForAnalysis({1, 1, 1, 1}) == 0xE8, "three tangent plane crossings");
        Check(zero.SphereMaskForAnalysis({0, 0, 0, 0}) == 255, "pivot zero sphere all octants");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        Check(spOctreeNode::OctantForAnalysis({nan, nan, nan}, {}) == 0, "unordered low octant");
        spOctreeNode::PlaneSet planes{{{{{1, 0, 0, 1}}, true}}, 1};
        auto word = zero.VisibleChildrenForAnalysis(planes, {});
        Check((word & 15) == 4, "positive X clips four lower children");
        planes.planes[0].equation[3] = -1;
        zero.ReducePlanesForChildForAnalysis(1, planes);
        Check(planes.activeCount == 0 && !planes.planes[0].enabled, "inside child disables plane");
        Check(spOctreeNode::TraversalOrderForAnalysis(0) == 0xFAB888 &&
                  spOctreeNode::TraversalOrderForAnalysis(7) == 0x08C777,
              "original nearest-first table");
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
            UnitTests();
            std::cout << "PASS " << checks << '/' << checks << ": Octree query slice\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
