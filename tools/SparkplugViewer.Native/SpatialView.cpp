// Host inspection DTO. Only getters from actual loaded resources are used.
// Float bits preserve NaN/signed-zero without a JSON numeric conversion.
#include "ResourceGraph.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spBSPNode.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include "Code/Sparkplug/spPartitionSystem.h"
#include "Code/Sparkplug/spPartitionRenderable.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spZonePortal.h"
#include "Code/Sparkplug/spZonePortalNode.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace spvhost {
namespace {
using namespace sparkplug::reconstruction;
class JSON {
public:
    std::string value;
    void Add(const char* text){value+=text;Check();}
    void Number(std::uint32_t number){value+=std::to_string(number);Check();}
    void Check(){if(value.size()>16u*1024u*1024u)throw std::runtime_error("Spatial snapshot exceeds 16 MiB host limit");}
    template<class Values,class Project> void Array(const Values& values,Project project){
        Add("[");bool first=true;for(const auto& item:values){if(!first)Add(",");first=false;project(item);}Add("]");}
    template<class Values> void Bits(const Values& values){Array(values,[&](float n){std::uint32_t bits;std::memcpy(&bits,&n,4);Number(bits);});}
};
}
const std::string& ResourceGraph::SpatialJSON() const {
    if(spatialJSON)return *spatialJSON;
    JSON j;
    auto references=[&](const auto& items){j.Array(items,[&](const auto& p){j.Number(ID(p.get()));});};
    auto borrowed=[&](const auto& items){j.Array(items,[&](const auto* p){j.Number(ID(p));});};
    auto resources=[&](auto* type,const char* name,auto project){
        using T=std::remove_pointer_t<decltype(type)>;
        j.Add(name);j.Add(":[");bool first=true;
        for(const auto& entry:entries){auto* object=dynamic_cast<T*>(entry.object);if(!object||ID(object)!=entry.id)continue;
            if(!first)j.Add(",");first=false;j.Add("{\"Id\":");j.Number(entry.id);project(*object);j.Add("}");}
        j.Add("]");};
    j.Add("{");
    resources(static_cast<spPartitionNode*>(nullptr),"\"Partitions\"",[&](auto& node){
        j.Add(",\"Color\":");j.Number(node.GetDebugColorForAnalysis());
        j.Add(",\"System\":");j.Number(ID(node.GetPartitionSystemForAnalysis()));
        j.Add(",\"Zone\":");j.Number(ID(node.GetZoneForAnalysis()));
        j.Add(",\"Parent\":");j.Number(ID(node.GetParentForAnalysis()));
        j.Add(",\"Children\":[");for(std::size_t slot=0;slot<node.GetChildCountForAnalysis();++slot){
            if(slot)j.Add(",");j.Number(ID(node.GetChildForAnalysis(slot)));}j.Add("]");
        j.Add(",\"Collisions\":");borrowed(node.GetCollisionsForAnalysis());
        j.Add(",\"Portals\":");references(node.GetPortalsForAnalysis());
        j.Add(",\"Statics\":");references(node.GetStaticObjectsForAnalysis());
        j.Add(",\"Payload\":");j.Number(ID(node.GetPartitionRenderableForAnalysis()));
        j.Add(",\"PlaneBits\":");auto* bsp=dynamic_cast<spBSPNode*>(&node);
        if(bsp&&bsp->HasPlaneForAnalysis())j.Bits(bsp->GetPlane());else j.Add("null");
        j.Add(",\"PolygonBits\":[");if(bsp)for(std::uint32_t i=0;i<bsp->GetPolygonVertexCount();++i){
            if(i)j.Add(",");j.Bits(bsp->GetPolygonVertex(i));}j.Add("]");
        auto* octree=dynamic_cast<spOctreeNode*>(&node);
        j.Add(",\"PivotBits\":");if(octree&&octree->HasGeometryForAnalysis())j.Bits(octree->GetPivotForAnalysis());else j.Add("null");
        j.Add(",\"MinimumBits\":");if(octree)j.Bits(octree->GetMinsForAnalysis());else j.Add("null");
        j.Add(",\"MaximumBits\":");if(octree)j.Bits(octree->GetMaxsForAnalysis());else j.Add("null");
    });
    j.Add(",");resources(static_cast<spPartitionSystem*>(nullptr),"\"Systems\"",[&](auto& system){
        j.Add(",\"Root\":");j.Number(ID(system.GetPartitionRootForAnalysis()));
        j.Add(",\"Renderables\":[");for(std::size_t i=0;i<system.GetRenderableCountForAnalysis();++i){
            if(i)j.Add(",");j.Number(ID(system.GetRenderableForAnalysis(i)));}j.Add("]");});
    j.Add(",");resources(static_cast<spPartitionRenderable*>(nullptr),"\"Payloads\"",[&](auto& payload){
        j.Add(",\"Color\":");j.Number(payload.GetDebugColorForAnalysis());
        j.Add(",\"Renderables\":");references(payload.GetRenderablesForAnalysis());});
    j.Add(",");resources(static_cast<spZone*>(nullptr),"\"Zones\"",[&](auto& zone){
        j.Add(",\"Roots\":");borrowed(zone.GetRootsForAnalysis());});
    j.Add(",");resources(static_cast<spZonePortal*>(nullptr),"\"Portals\"",[&](auto& portal){
        j.Add(",\"Destination\":");j.Number(ID(portal.GetDestinationZone()));
        j.Add(",\"Open\":");j.Number(portal.GetOpenByteForAnalysis());
        j.Add(",\"PolygonBits\":");j.Array(portal.GetPolygonForAnalysis(),[&](const auto& point){j.Bits(point);});
        j.Add(",\"PlaneBits\":");if(portal.HasPlaneForAnalysis())j.Bits(portal.GetPlaneForAnalysis());else j.Add("null");});
    j.Add(",");resources(static_cast<spZonePortalNode*>(nullptr),"\"PortalNodes\"",[&](auto& node){
        j.Add(",\"Portals\":");borrowed(node.GetPortalsForAnalysis());});
    j.Add("}");spatialJSON=std::move(j.value);return *spatialJSON;
}
}
