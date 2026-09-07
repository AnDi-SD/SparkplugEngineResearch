#include "Code/Sparkplug/spZonePortalNode.h"
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
    float FromBits(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        return bits;
    }
    void Batch()
    {
        std::uint32_t first;
        while (std::cin >> first)
        {
            std::array<float, 9> points{};
            points[0] = FromBits(first);
            for (std::size_t i = 1; i < 9; ++i)
            {
                std::uint32_t word;
                if (!(std::cin >> word))
                    throw std::runtime_error("truncated input");
                points[i] = FromBits(word);
            }
            const auto plane = spZonePortal::PlaneFromFirstThreeForAnalysis(
                {points[0], points[1], points[2]}, {points[3], points[4], points[5]},
                {points[6], points[7], points[8]});
            std::cout << Bits(plane[0]) << ',' << Bits(plane[1]) << ',' << Bits(plane[2]) << ','
                      << Bits(plane[3]) << '\n';
        }
    }
    void Units()
    {
        spZonePortal portal;
        Check(portal.IsOpen() && portal.GetPolygonForAnalysis().empty() &&
                  !portal.HasPlaneForAnalysis() && !portal.GetDestinationZone(),
              "ctor defaults and explicit plane gap");
        Check(portal.IsKindOf(spNamedObject::ClassID) && !portal.IsKindOf(spNode::ClassID),
              "original Named, not Node");
        const std::vector<spZonePortal::Vector3> quad{
            {-2, -2, 10}, {2, -2, 10}, {2, 2, 10}, {-2, 2, 10}};
        Check(portal.SetPolygonForAnalysis(quad), "quad init");
        Check(portal.GetPlaneForAnalysis() == spZonePortal::Plane{0, 0, 1, 10},
              "forward unit plane");
        Check(portal.GetPolygonForAnalysis() == quad && &portal.GetPolygonForAnalysis() != &quad,
              "owned copy");
        Check(portal.GetPolygonVertexCount() == 4 && portal.GetPolygonVertex(2) == quad[2],
              "original diagnostic getter names");
        Check(portal.SetPolygonForAnalysis(portal.GetPolygonForAnalysis()), "host-safe self input");
        auto malformed = quad;
        malformed[0][0] = std::numeric_limits<float>::quiet_NaN();
        Check(!portal.SetPolygonForAnalysis(malformed) && portal.GetPolygonForAnalysis() == quad,
              "host finite guard preserves state");
        Check(!portal.SetPolygonForAnalysis({}) && portal.HasPlaneForAnalysis(),
              "host short guard preserves state");
        auto nonplanar = quad;
        nonplanar[3] = {100, 200, 300};
        Check(portal.SetPolygonForAnalysis(nonplanar) &&
                  portal.GetPlaneForAnalysis() == spZonePortal::Plane{0, 0, 1, 10},
              "tail does not alter first-three plane");
        Check(portal.SetPolygonForAnalysis({{1, 2, 3}, {2, 4, 6}, {3, 6, 9}}) &&
                  portal.GetPlaneForAnalysis() == spZonePortal::Plane{},
              "degenerate accepted as zero plane");
        auto* destination = reinterpret_cast<spZone*>(std::uintptr_t(0x1234));
        portal.SetDestinationForAnalysis(destination);
        portal.SetOpenForAnalysis(false);
        portal.SetVisibilityMarkForAnalysis(75);
        Check(portal.GetDestinationZone() == destination && !portal.IsOpen() &&
                  portal.GetVisibilityMarkForAnalysis() == 75,
              "borrowed raw state access");
        portal.SetName("door");
        auto clone = portal.Clone();
        auto* typed = dynamic_cast<spZonePortal*>(clone.get());
        Check(typed && std::string(typed->GetName()) == "door" && typed->IsOpen() &&
                  !typed->HasPlaneForAnalysis() && !typed->GetDestinationZone() &&
                  typed->GetVisibilityMarkForAnalysis() == 0,
              "Named-only clone fresh own state");
        spZonePortalNode node;
        Check(node.IsKindOf(spNode::ClassID) && node.GetPortalsForAnalysis().empty(),
              "portal node original identity/default");
        Check(node.AppendPortalForAnalysis(&portal) && node.AppendPortalForAnalysis(&portal) &&
                  node.AppendPortalForAnalysis(nullptr) &&
                  node.GetPortalsForAnalysis() ==
                      std::vector<spZonePortal*>{&portal, &portal, nullptr},
              "raw append preserves duplicate/null");
        node.SetPositionForAnalysis({80, 90, 100});
        node.MarkLocalTransformDirtyForAnalysis();
        Check(node.UpdateWorldForAnalysis() &&
                  portal.GetPlaneForAnalysis() == spZonePortal::Plane{},
              "Node world does not transform portal");
        node.SetEnabledForAnalysis(false);
        Check(!portal.IsOpen(), "Enabled independent of Open");
        Check(node.GetZonePortal(0) == &portal && node.GetZonePortal(2) == nullptr &&
                  node.GetZonePortal(99) == nullptr,
              "original getter name with host bounds guard");
        auto nodeClone = node.Clone();
        auto* typedNode = dynamic_cast<spZonePortalNode*>(nodeClone.get());
        Check(typedNode && typedNode->GetPortalsForAnalysis().empty() &&
                  typedNode->GetPositionForAnalysis() == node.GetPositionForAnalysis(),
              "Node-only clone omits borrowed portals");
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
            std::cout << "PASS " << checks << '/' << checks << ": ZonePortal source\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
