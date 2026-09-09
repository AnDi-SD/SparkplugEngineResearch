// Host DTO only: all values are copied from the loaded reconstructed objects.
// No SMO parser, pathfinding, graph validation or game writer lives here.
#include "ResourceGraph.h"
#include "Code/Sparkplug/spNavigationGraph.h"
#include "Code/Sparkplug/spNavigationPortal.h"
#include "Code/Sparkplug/spMeshNavigationSet.h"
#include <cstring>
#include <stdexcept>

namespace spvhost {
namespace {
using namespace sparkplug::reconstruction;
class JSON {
public:
    std::string value;
    void Add(const char* text){value+=text;Check();}
    void Number(std::uint32_t number){value+=std::to_string(number);Check();}
    void Check(){if(value.size()>16u*1024u*1024u)throw std::runtime_error("Navigation snapshot exceeds 16 MiB host limit");}
    template<class Values,class Project> void Array(const Values& values,Project project){
        Add("[");bool first=true;for(const auto& item:values){if(!first)Add(",");first=false;project(item);}Add("]");}
    template<class Values> void Numbers(const Values& values){Array(values,[&](auto n){Number(n);});}
    template<class Values> void Bits(const Values& values){Array(values,[&](float n){std::uint32_t bits;std::memcpy(&bits,&n,4);Number(bits);});}
    void Table(const spNavigationSet::Table* table){
        if(!table){Add("null");return;}Add("{\"Rows\":");Number(table->rows);Add(",\"Columns\":");Number(table->columns);Add(",\"Values\":[");
        bool first=true;for(std::uint32_t r=0;r<table->rows;++r)for(std::uint32_t c=0;c<table->columns;++c){
            if(!first)Add(",");first=false;Number(table->Get(r,c));}Add("]}");}
};
}
const std::string& ResourceGraph::NavigationJSON() const {
    if(navigationJSON)return *navigationJSON;
    JSON j;j.Add("{\"Sets\":[");bool first=true;
    for(const auto& entry:entries){
        const auto* set=dynamic_cast<const spMeshNavigationSet*>(entry.object);if(!set||ID(set)!=entry.id)continue;
        if(!first)j.Add(",");first=false;j.Add("{\"Id\":");j.Number(entry.id);
        j.Add(",\"NodeCount\":");j.Number(set->GetNodeCountForAnalysis());
        j.Add(",\"Index\":");j.Number(set->GetGraphIndexForAnalysis());j.Add(",\"Enabled\":");j.Number(set->GetNavigationEnabledForAnalysis());
        j.Add(",\"Transitions\":");j.Table(set->GetTransitionsForAnalysis());j.Add(",\"PortalTransitions\":");j.Table(set->GetPortalTransitionsForAnalysis());
        j.Add(",\"Neighbours\":");j.Array(set->GetNeighboursForAnalysis(),[&](const auto& row){j.Numbers(row);});
        j.Add(",\"Portals\":");j.Array(set->GetPortalsForAnalysis(),[&](auto* p){j.Number(ID(p));});
        j.Add(",\"Mesh\":");j.Number(ID(set->GetMeshForAnalysis()));
        j.Add(",\"MinimumBits\":");j.Bits(set->GetMinimumForAnalysis());j.Add(",\"MaximumBits\":");j.Bits(set->GetMaximumForAnalysis());
        j.Add(",\"SphereBits\":");j.Bits(set->GetNavigationSphereForAnalysis());j.Add("}");
    }
    j.Add("],\"Graphs\":[");first=true;
    for(const auto& entry:entries){
        const auto* graph=dynamic_cast<const spNavigationGraph*>(entry.object);if(!graph||ID(graph)!=entry.id)continue;
        if(!first)j.Add(",");first=false;j.Add("{\"Id\":");j.Number(entry.id);
        j.Add(",\"Sets\":");j.Array(graph->GetSetsForAnalysis(),[&](auto* p){j.Number(ID(p));});
        j.Add(",\"Portals\":");j.Array(graph->GetPortalsForAnalysis(),[&](auto* p){j.Number(ID(p));});
        const auto& paths=graph->GetPathsForAnalysis();j.Add(",\"Size\":");j.Number(static_cast<std::uint32_t>(paths.size()));
        j.Add(",\"Paths\":[");bool firstPath=true;for(const auto& row:paths)for(const auto& path:row){
            if(!firstPath)j.Add(",");firstPath=false;j.Add("{\"Next\":");j.Number(path.nextPortal);
            j.Add(",\"Alternatives\":");j.Array(path.alternatives,[&](const auto& pair){j.Numbers(pair);});j.Add("}");}
        j.Add("]}");
    }
    j.Add("],\"Portals\":[");first=true;
    for(const auto& entry:entries){
        const auto* portal=dynamic_cast<const spNavigationPortal*>(entry.object);if(!portal||ID(portal)!=entry.id)continue;
        if(!first)j.Add(",");first=false;j.Add("{\"Id\":");j.Number(entry.id);
        j.Add(",\"Index\":");j.Number(portal->GetGraphIndexForAnalysis());j.Add(",\"Enabled\":");j.Number(portal->GetNavigationEnabledForAnalysis());
        j.Add(",\"Graph\":");if(portal->IsGraphKnownForAnalysis())j.Number(ID(portal->GetGraphForAnalysis()));else j.Add("null");
        j.Add(",\"Sets\":");if(portal->AreEndpointsKnownForAnalysis())j.Array(portal->GetEndpointsForAnalysis(),[&](auto* p){j.Number(ID(p));});else j.Add("null");
        j.Add(",\"FirstNodes\":");j.Numbers(portal->GetFirstNodesForAnalysis());j.Add(",\"SecondNodes\":");j.Numbers(portal->GetSecondNodesForAnalysis());
        j.Add(",\"Paths\":");j.Array(portal->GetPathsForAnalysis(),[&](const auto& row){j.Numbers(row);});j.Add("}");
    }
    j.Add("]}");navigationJSON=std::move(j.value);return *navigationJSON;
}
}
