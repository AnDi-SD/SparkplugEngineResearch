#include "Code/Sparkplug/spOcclusionVolume.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
int checks=0;
void Check(bool result,const char* label){++checks;if(!result)throw std::runtime_error(label);}
using Volume=spOcclusionVolume;
bool Capture(const std::string& mode,bool emit)
{
    Volume object;std::vector<Volume::Vector3> points{{0,0,0},{1,0,0},{2,0,0},{2,1,0}};
    std::vector<Volume::PlaneForAnalysis> planes{{0,0,1,0},{0,0,1,7}};
    std::vector<Volume::PreparedEdgeForAnalysis> input{{0,1,0,{}},{1,2,1,{}},{2,3,1,{}}};
    int entry=0x46e820;
    if(mode.rfind("convex:",0)==0){planes={{0,1,0,0},{0,0,std::stof(mode.substr(7)),0}};input={{0,1,0,1}};entry=0x46d670;}
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
        std::cout<<"]}\n";
    }
    return result;
}
}
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[1])=="--capture"){Capture(argv[2],true);return 0;}
    Volume object;Check(object.IsKindOf(Volume::ClassID)&&object.IsKindOf(spNode::ClassID),"actual RTTI and direct Node base");
    Check(object.GetPlanarByteForAnalysis()==0&&object.GetBorderCountForAnalysis()==0,"actual constructor geometry defaults");
    Check(object.CheckPlanarityForAnalysis(),"zero border count passes native leaf without Init");
    for(const auto* mode:{"convex:0","convex:1","link:two","link:three","remove:distance","remove:normal","planar:same","planar:near","planar:closed","merge:chain","merge:turn","merge:zero"})
        Check(Capture(mode,false),"original positive geometry leaf");
    for(const auto* mode:{"convex:-1","link:concave","planar:far"})Check(!Capture(mode,false),"original rejecting leaf");
    Check(!object.SetPreparedTopologyForAnalysis({{0,0,0}},{{0,0,1,0}},{{0,1,0,{}}},1,0),"invalid prepared pointer index");
    Check(object.SetPreparedTopologyForAnalysis({{0,0,0},{1,0,0}},{{0,0,1,0}},{{0,1,0,{}}},1,1),"clone source owns prepared topology");
    object.SetPositionForAnalysis({1,2,3});spCloneManager clones;auto clone=clones.Clone(object);
    auto* volume=dynamic_cast<Volume*>(clone.get());Check(volume&&volume->GetPositionForAnalysis()==object.GetPositionForAnalysis(),"actual Node-only clone keeps concrete class and Node position");
    Check(volume->GetEdgesForAnalysis().empty()&&volume->GetFacesForAnalysis().empty()&&volume->GetBorderCountForAnalysis()==0&&volume->GetPlanarByteForAnalysis()==0,"original clone omits own geometry");
    std::cout<<"PASS "<<checks<<"/"<<checks<<": shared Occlusion topology leaves (full Init remains open)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
