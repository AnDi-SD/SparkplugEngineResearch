#include "Code/Sparkplug/spOcclusionVolume.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
int checks=0;
void Check(bool result,const char* label){++checks;if(!result)throw std::runtime_error(label);}
using Volume=spOcclusionVolume;
std::vector<Volume::Vector3> RacePoints()
{
    // Unchanged race_02.smo resource8 VB payload190055, actual PC readers and
    // shape/Init/reader captures in occlusion-init-next,2026-09-10. No asset in Git.
    return {{-4958.6494140625F,-82.49378204345703F,724.2744750976562F},
        {-4958.6494140625F,-82.49374389648438F,-2728.739990234375F},
        {4958.6494140625F,650.8276977539062F,724.2744750976562F},
        {4958.6494140625F,650.8276977539062F,-2728.739990234375F}};
}
void CheckFace(const Volume& object,std::size_t index,const std::array<std::uint32_t,4>& bits,
    const std::array<std::size_t,3>& positions)
{
    const auto& face=object.GetFacesForAnalysis().at(index);
    for(std::size_t i=0;i<4;++i)
    {
        std::uint32_t actual=0;std::memcpy(&actual,&face.plane[i],sizeof(actual));
        Check(actual==bits[i],"actual PC face plane bits");
    }
    for(std::size_t i=0;i<3;++i)Check(face.positions[i]==&object.GetPointsForAnalysis()[positions[i]],"first matched face retains original position pointers");
    Check(!face.cameraSideKnown,"unwritten original camera-side word stays unknown");
}
void CheckShapeProducerAndDriver()
{
    const std::vector<std::uint16_t> indices{2,1,0,1,2,3};
    const std::array<std::uint32_t,4> front{0xBD97063A,0x3F7F4D91,0x32976BE6,0x438DB257};
    const std::array<std::uint32_t,4> back{0x3D97063A,0xBF7F4D91,0xB23D46DF,0xC38DB257};
    Volume producer;
    Check(producer.SetShapeBuffersForAnalysis(RacePoints(),indices),"explicit actual race02 shape input");
    Check(producer.BuildFacesAndEdgesForAnalysis(),"actual470B20 producer returns true");
    Check(producer.GetFacesForAnalysis().size()==1&&producer.GetEdgesForAnalysis().size()==6,
        "original checkpoint before46E820 has one face and six edges");
    CheckFace(producer,0,front,{2,1,0});
    const std::size_t initialEdges[6][2]={{2,1},{1,0},{0,2},{1,2},{2,3},{3,1}};
    for(std::size_t i=0;i<6;++i)
    {
        const auto& edge=*producer.GetEdgesForAnalysis()[i];
        Check(edge.start==&producer.GetPointsForAnalysis()[initialEdges[i][0]]&&
            edge.end==&producer.GetPointsForAnalysis()[initialEdges[i][1]],"original triangle/edge append order");
        Check(edge.own==&producer.GetFacesForAnalysis()[0]&&!edge.opposite&&edge.border==1&&
            edge.walkStamp==0&&edge.outgoing.empty(),"original initial edge assignments");
    }
    Check(producer.GetBorderCountForAnalysis()==6&&producer.GetPlanarByteForAnalysis()==0,"producer sets border count only");

    Volume shape;
    Check(shape.SetShapeBuffersForAnalysis(RacePoints(),indices),"fresh driver receives source once");
    Check(shape.BuildShapeForAnalysis(),"actual470E30 race02 result true");
    Check(shape.GetFacesForAnalysis().size()==2&&shape.GetEdgesForAnalysis().size()==8,
        "original final topology has two faces and eight edges");
    CheckFace(shape,0,front,{2,1,0});CheckFace(shape,1,back,{0,1,2});
    const std::size_t finalEdges[8][2]={{3,1},{1,0},{0,2},{2,3},{3,2},{2,0},{0,1},{1,3}};
    const std::vector<std::vector<std::size_t>> outgoing{{1},{2},{3},{0},{3,5},{2,6},{1,7},{0,4}};
    for(std::size_t i=0;i<8;++i)
    {
        const auto& edge=*shape.GetEdgesForAnalysis()[i];
        Check(edge.start==&shape.GetPointsForAnalysis()[finalEdges[i][0]]&&
            edge.end==&shape.GetPointsForAnalysis()[finalEdges[i][1]],"original compacted and reverse edge order");
        Check(edge.own==&shape.GetFacesForAnalysis()[i<4?0:1]&&!edge.opposite&&edge.border==1&&edge.walkStamp==0,
            "original final own/opposite/border/stamp assignments");
        std::vector<Volume::EdgeForAnalysis*> expected;
        for(auto target:outgoing[i])expected.push_back(shape.GetEdgesForAnalysis()[target].get());
        Check(edge.outgoing==expected,"original final outgoing order retains forward lists");
    }
    Check(shape.GetBorderCountForAnalysis()==4&&shape.GetPlanarByteForAnalysis()==1,
        "planar reverse side does not double border count");

    // Addressed host guards preserve a completed analysis state; these are not
    // claims that the original unchecked buffer API rejects malformed input.
    Check(!shape.SetShapeBuffersForAnalysis(RacePoints(),{0,1}),"host rejects incomplete triangle");
    Check(!shape.SetShapeBuffersForAnalysis(RacePoints(),{0,1,4}),"host rejects invalid vertex index");
    auto invalid=RacePoints();invalid[0][0]=std::numeric_limits<float>::quiet_NaN();
    Check(!shape.SetShapeBuffersForAnalysis(std::move(invalid),indices),"host requires finite positions");
    Check(shape.GetFacesForAnalysis().size()==2&&shape.GetEdgesForAnalysis().size()==8,
        "invalid host preparation preserves prior shape");
    Volume empty;Check(!empty.BuildShapeForAnalysis(),"missing prepared buffers are unsupported");
    Check(empty.SetShapeBuffersForAnalysis({},{}),"explicit empty producer input");
    Check(empty.BuildFacesAndEdgesForAnalysis()&&empty.GetBorderCountForAnalysis()==0,
        "PC13B4F72 zero-triangle producer skips loop and returns true");
    Check(!empty.BuildShapeForAnalysis(),"host prevents original unchecked empty first-edge access");

    // Original fresh-init-batch-run1 positive-u32 returns AL1, but reverse
    // face growth leaves the first three edge.own pointers outside the current
    // face vector. This host refusal does not claim an original game failure.
    Volume single;
    Check(single.SetShapeBuffersForAnalysis({{1,2,3},{4,2,3},{1,6,3}},{0,1,2}),
        "original positive-u32 positions and narrowed indices");
    Check(!single.BuildShapeForAnalysis(),"single-triangle full shape is explicitly unsupported");
    Check(single.GetFacesForAnalysis().empty()&&single.GetEdgesForAnalysis().empty(),
        "unsupported shape guard precedes topology mutation");
    Check(single.BuildFacesAndEdgesForAnalysis(),"single-triangle producer remains available");
    Check(single.GetFacesForAnalysis().size()==1&&single.GetEdgesForAnalysis().size()==3,
        "single-triangle producer does not append reverse side");
    Check(std::all_of(single.GetEdgesForAnalysis().begin(),single.GetEdgesForAnalysis().end(),
        [&](const auto& edge){return edge->own==&single.GetFacesForAnalysis()[0];}),
        "producer own-face pointers belong to its current face storage");

    // PC13D0533 branches to false after a rejecting opposite-edge link. This
    // small input targets composition/partial mutation; leaf contracts already
    // have independent original comparisons above.
    Volume rejected;
    Check(rejected.SetShapeBuffersForAnalysis({{0,0,0},{1,0,0},{0,1,0},{0,0,1}},
        {0,1,2,1,0,3},19),"explicit concave-link driver input");
    Check(!rejected.BuildShapeForAnalysis(),"driver propagates original link rejection");
    Check(rejected.GetFacesForAnalysis().size()==2&&rejected.GetEdgesForAnalysis().size()==6&&
        rejected.GetBorderCountForAnalysis()==4&&rejected.GetPlanarByteForAnalysis()==0,
        "rejected driver retains producer and partial link mutations");
    Check(rejected.GetEdgesForAnalysis()[0]->opposite==&rejected.GetFacesForAnalysis()[1]&&
        rejected.GetEdgesForAnalysis()[3]->opposite==&rejected.GetFacesForAnalysis()[0]&&
        rejected.GetEdgesForAnalysis()[0]->walkStamp==19,
        "link assignments and original walk-stamp input survive failure");
}
bool Capture(const std::string& mode,bool emit)
{
    Volume object;std::vector<Volume::Vector3> points{{0,0,0},{1,0,0},{2,0,0},{2,1,0}};
    std::vector<Volume::PlaneForAnalysis> planes{{0,0,1,0},{0,0,1,7}};
    std::vector<Volume::PreparedEdgeForAnalysis> input{{0,1,0,{}},{1,2,1,{}},{2,3,1,{}}};
    int entry=0x46e820;
    if(mode.rfind("connect:",0)==0)
    {
        points={{0,0,0},{1,0,0},{1,1,0},{1,-1,0},{2,0,0},{1.0005F,0,0},{1.002F,0,0}};
        planes={{0,0,1,0},{0,0,1,0}};entry=0x4705a0;input={{0,1,0,{}}};
        if(mode=="connect:turn")input.push_back({1,2,0,{}});
        else if(mode=="connect:concave")input.push_back({1,3,0,{}});
        else if(mode=="connect:collinear")input.push_back({1,4,0,{}});
        else if(mode=="connect:cycle"){input.push_back({1,2,0,{}});input.push_back({2,0,0,{}});}
        else if(mode=="connect:reverse")input.push_back({1,0,0,{}});
        else if(mode=="connect:internal"){input[0].opposite=1;input.push_back({1,3,0,{}});}
        else if(mode=="connect:zero")input.push_back({1,1,0,{}});
        else if(mode=="connect:tiny")input.push_back({1,5,0,{}});
        else if(mode=="connect:near")input.push_back({5,2,0,{}});
        else if(mode=="connect:far")input.push_back({6,2,0,{}});
        else if(mode=="connect:partial"){input.push_back({1,2,0,{}});input.push_back({1,3,0,{}});}
        else Check(mode=="connect:open","known connection capture");
    }
    else if(mode.rfind("convex:",0)==0){planes={{0,1,0,0},{0,0,std::stof(mode.substr(7)),0}};input={{0,1,0,1}};entry=0x46d670;}
    else if(mode.rfind("link:",0)==0){input={{0,1,0,{}},{1,0,1,{}}};entry=0x46e1a0;
        if(mode=="link:three")input.push_back({1,0,1,{}});
        if(mode=="link:concave")planes={{0,1,0,0},{0,0,-1,0}};}
    else if(mode.rfind("remove:",0)==0){input={{0,1,0,1},{1,0,1,0},{2,3,1,{}}};entry=0x46eb10;
        if(mode=="remove:normal")planes[1]={0,.01F,1,0};}
    else if(mode.rfind("planar:",0)==0){entry=0x46e3b0;
        if(mode=="planar:same")planes[1]=planes[0];if(mode=="planar:near")planes[1]={0,0,1,.0005F};}
    else if(mode=="merge:turn")input={{0,1,0,{}},{1,3,1,{}}};
    else if(mode=="merge:zero")input={{0,0,0,{}},{0,1,1,{}}};
    else Check(mode=="merge:chain","known capture mode");
    Check(object.SetPreparedTopologyForAnalysis(std::move(points),planes,input,
        mode=="planar:closed"?0:static_cast<std::uint32_t>(input.size()),127),"explicit prepared topology");
    std::vector<const Volume::EdgeForAnalysis*> before;for(const auto& edge:object.GetEdgesForAnalysis())before.push_back(edge.get());
    bool result=false;
    switch(entry){case 0x46d670:result=object.IsEdgeConvexForAnalysis(*object.GetEdgesForAnalysis()[0]);break;
        case 0x46e1a0:result=object.LinkOppositeEdgesForAnalysis();break;
        case 0x46eb10:result=object.RemoveCoplanarEdgesForAnalysis();break;
        case 0x46e3b0:result=object.CheckPlanarityForAnalysis();break;
        case 0x4705a0:result=object.ConnectOutgoingEdgesForAnalysis(*object.GetEdgesForAnalysis()[0]);break;
        default:result=object.MergeCollinearEdgesForAnalysis();}
    if(emit)
    {
        std::cout<<"{\"mode\":\""<<mode<<"\",\"entry\":\"0x"<<std::hex<<entry<<std::dec<<"\",\"result\":"<<result<<",\"border_count\":"<<object.GetBorderCountForAnalysis()
            <<",\"planar\":"<<unsigned(object.GetPlanarByteForAnalysis())<<",\"edges\":[";
        bool comma=false;for(const auto& edge:object.GetEdgesForAnalysis())
        {
            if(comma)std::cout<<',';comma=true;
            std::cout<<"{\"original_index\":"<<(std::find(before.begin(),before.end(),edge.get())-before.begin())
                <<",\"start\":"<<(edge->start-object.GetPointsForAnalysis().data())<<",\"end\":"<<(edge->end-object.GetPointsForAnalysis().data())
                <<",\"own\":"<<(edge->own-object.GetFacesForAnalysis().data())<<",\"opposite\":"<<(edge->opposite?edge->opposite-object.GetFacesForAnalysis().data():-1)
                <<",\"border\":"<<unsigned(edge->border)<<'}';
        }
        std::cout<<"],\"deleted_edges\":[";comma=false;
        for(std::size_t i=0;i<before.size();++i)if(std::none_of(object.GetEdgesForAnalysis().begin(),object.GetEdgesForAnalysis().end(),[&](const auto& edge){return edge.get()==before[i];}))
        {if(comma)std::cout<<',';comma=true;std::cout<<i;}
        std::cout<<"]";
        if(entry==0x4705a0)
        {
            std::cout<<",\"outgoing\":[";comma=false;
            for(const auto& edge:object.GetEdgesForAnalysis())
            {
                if(comma)std::cout<<',';comma=true;std::cout<<'[';bool inner=false;
                for(const auto* next:edge->outgoing){if(inner)std::cout<<',';inner=true;std::cout<<(std::find(before.begin(),before.end(),next)-before.begin());}
                std::cout<<']';
            }
            std::cout<<"],\"repeat_result\":"<<object.ConnectOutgoingEdgesForAnalysis(*object.GetEdgesForAnalysis()[0]);
        }
        std::cout<<"}\n";
    }
    return result;
}
}
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[1])=="--capture"){Capture(argv[2],true);return 0;}
    CheckShapeProducerAndDriver();
    Volume object;Check(object.IsKindOf(Volume::ClassID)&&object.IsKindOf(spNode::ClassID),"actual RTTI and direct Node base");
    Check(object.GetPlanarByteForAnalysis()==0&&object.GetBorderCountForAnalysis()==0,"actual constructor geometry defaults");
    Check(object.CheckPlanarityForAnalysis(),"zero border count passes native leaf without Init");
    for(const auto* mode:{"convex:0","convex:1","link:two","link:three","remove:distance","remove:normal","planar:same","planar:near","planar:closed","merge:chain","merge:turn","merge:zero"})
        Check(Capture(mode,false),"original positive geometry leaf");
    for(const auto* mode:{"convex:-1","link:concave","planar:far"})Check(!Capture(mode,false),"original rejecting leaf");
    for(const auto* mode:{"open","turn","cycle","reverse","internal","zero","tiny","near","far"})
        Check(Capture(std::string("connect:")+mode,false),"original outgoing graph acceptance");
    for(const auto* mode:{"concave","collinear","partial"})
        Check(!Capture(std::string("connect:")+mode,false),"original outgoing graph rejection");
    Check(!object.SetPreparedTopologyForAnalysis({{0,0,0}},{{0,0,1,0}},{{0,1,0,{}}},1,0),"invalid prepared pointer index");
    Check(object.SetPreparedTopologyForAnalysis({{0,0,0},{1,0,0}},{{0,0,1,0}},{{0,1,0,{}}},1,1),"clone source owns prepared topology");
    object.SetPositionForAnalysis({1,2,3});spCloneManager clones;auto clone=clones.Clone(object);
    auto* volume=dynamic_cast<Volume*>(clone.get());Check(volume&&volume->GetPositionForAnalysis()==object.GetPositionForAnalysis(),"actual Node-only clone keeps concrete class and Node position");
    Check(volume->GetEdgesForAnalysis().empty()&&volume->GetFacesForAnalysis().empty()&&volume->GetBorderCountForAnalysis()==0&&volume->GetPlanarByteForAnalysis()==0,"original clone omits own geometry");
    std::cout<<"PASS "<<checks<<"/"<<checks<<": shared Occlusion shape/topology (runtime Init not implemented)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
