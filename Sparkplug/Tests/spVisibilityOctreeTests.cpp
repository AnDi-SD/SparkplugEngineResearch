// Captured original frame inputs exercise the shared traversal; no replacement
// callback supplies a child order, clipping decision, or successful traversal.
#include "Code/Sparkplug/spVisibilityManager.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include "Code/Sparkplug/spZone.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    using M=spVisibilityManager;
    unsigned checks=0;
    void Check(bool condition,const char* message)
    {
        ++checks;
        if(!condition)throw std::runtime_error(message);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t result;std::memcpy(&result,&value,4);return result;
    }
    float ReadFloat()
    {
        std::uint32_t value=0;Check(bool(std::cin>>value),"missing float word");
        float result;std::memcpy(&result,&value,4);return result;
    }
    void Batch()
    {
        unsigned seed,cull,debug,nodeCount,supportCount;
        while(std::cin>>seed>>cull>>debug>>nodeCount>>supportCount)
        {
            Check(nodeCount&&nodeCount<=64&&supportCount<=128,"bounded graph input");
            M::TraversalCameraForAnalysis camera;
            for(auto& value:camera.position)value=ReadFloat();
            for(auto& value:camera.forward)value=ReadFloat();
            for(auto& value:camera.farPlane.equation)value=ReadFloat();
            for(auto& plane:camera.sides)for(auto& value:plane.equation)value=ReadFloat();
            std::vector<std::unique_ptr<spPartitionNode>> owned(nodeCount);
            std::vector<spPartitionNode*> nodes(nodeCount);
            std::vector<unsigned> parents(nodeCount),slots(nodeCount);
            std::vector<M::TraversalNodeForAnalysis> views(nodeCount);
            for(unsigned i=0;i<nodeCount;++i)
            {
                unsigned octree,mark;
                Check(bool(std::cin>>octree>>parents[i]>>slots[i]>>mark),"missing node");
                std::array<float,3> pivot,mins,maxs;
                for(auto& value:pivot)value=ReadFloat();
                for(auto& value:mins)value=ReadFloat();
                for(auto& value:maxs)value=ReadFloat();
                if(octree)
                {
                    auto spatial=std::make_unique<spOctreeNode>();
                    Check(spatial->SetGeometryForAnalysis(pivot,mins,maxs),"finite ordered geometry");
                    owned[i]=std::move(spatial);
                }
                else owned[i]=std::make_unique<spPartitionNode>();
                nodes[i]=owned[i].get();views[i].spatial=nodes[i];views[i].visibilityMark=mark;
                Check(i==0||(parents[i]<i&&slots[i]<8),"topologically ordered owned child");
            }
            for(unsigned i=nodeCount-1;i>0;--i)
            {
                auto* parent=dynamic_cast<spOctreeNode*>(nodes[parents[i]]);
                Check(parent&&!parent->GetChildForAnalysis(slots[i]),"unique Octree child slot");
                Check(parent->SetChildForAnalysis(slots[i],std::move(owned[i])),"child ownership transfer");
            }
            auto zone=std::make_shared<spZone>();nodes[0]->SetZoneForAnalysis(zone);
            Check(zone->AppendRootForAnalysis(nodes[0]),"borrowed root membership");
            std::vector<M::SupportForAnalysis> supports(supportCount);
            for(auto& support:supports)
            {
                unsigned node,enabled,bypass;
                Check(bool(std::cin>>node>>support.visibilityMark>>enabled>>bypass)&&node<nodeCount,"support membership");
                support.kind=M::SupportKindForAnalysis::RenderNode;
                support.enabled=enabled!=0;support.cullBypass=bypass!=0;
                for(auto& value:support.worldSphere)value=ReadFloat();
                views[node].dynamics.push_back(&support);
            }
            Check(bool(std::cin),"complete input");
            M manager;manager.SetFrameStampForAnalysis(seed);manager.SetSphereCullingForAnalysis(cull!=0);
            M::TraversalSceneForAnalysis scene;scene.partitionRoot=nodes[0];scene.unclippedDebug=debug!=0;
            std::vector<M::TraversalVisitForAnalysis> trace;scene.trace=&trace;
            for(auto& view:views)scene.nodes.push_back(&view);
            const auto status=manager.ProcessSceneForAnalysis(scene,&camera);
            std::cout<<static_cast<unsigned>(status)<<','<<scene.visibilityStamp;
            for(const auto& view:views)std::cout<<','<<view.visibilityMark;
            for(const auto& support:supports)std::cout<<','<<support.visibilityMark;
            const auto& visible=manager.GetVisibleForAnalysis();std::cout<<','<<visible.size();
            for(const auto* support:visible)std::cout<<','<<support-supports.data();
            std::cout<<','<<trace.size();
            for(const auto& entry:trace)
            {
                unsigned node=0;while(node<nodes.size()&&nodes[node]!=entry.spatial)++node;
                Check(node<nodes.size(),"trace points to represented node");
                std::cout<<','<<node<<','<<entry.unclipped<<','<<entry.planes.activeCount;
                for(float value:entry.incoming)std::cout<<','<<Bits(value);
                std::cout<<','<<entry.planes.planes.size();
                for(const auto& plane:entry.planes.planes)
                {
                    for(float value:plane.equation)std::cout<<','<<Bits(value);
                    std::cout<<','<<plane.enabled;
                }
            }
            std::cout<<'\n';
        }
    }
    void Tests()
    {
        // The original debug branch orders the eight octants in this sequence
        // for a camera in low XYZ; disabled render nodes are still appended.
        auto root=std::make_unique<spOctreeNode>();
        Check(root->SetGeometryForAnalysis({0,0,20},{-100,-100,0},{100,100,100}),"root geometry");
        auto zone=std::make_shared<spZone>();root->SetZoneForAnalysis(zone);
        Check(zone->AppendRootForAnalysis(root.get()),"zone root");
        std::array<M::TraversalNodeForAnalysis,9> views;views[0].spatial=root.get();
        std::array<M::SupportForAnalysis,8> supports;
        M::TraversalSceneForAnalysis scene;scene.partitionRoot=root.get();scene.unclippedDebug=true;
        scene.nodes.push_back(&views[0]);
        for(unsigned i=0;i<8;++i)
        {
            auto child=std::make_unique<spPartitionNode>();views[i+1].spatial=child.get();
            Check(root->SetChildForAnalysis(i,std::move(child)),"child");
            supports[i].kind=M::SupportKindForAnalysis::RenderNode;supports[i].enabled=false;
            views[i+1].dynamics={&supports[i]};scene.nodes.push_back(&views[i+1]);
        }
        M manager;M::TraversalCameraForAnalysis camera;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==M::TraversalStatusForAnalysis::Complete,"debug frame completes");
        const std::array<unsigned,8> expected{0,1,2,4,3,5,6,7};
        const auto& visible=manager.GetVisibleForAnalysis();Check(visible.size()==8,"disabled nodes included by debug branch");
        for(unsigned i=0;i<8;++i)
            Check(visible[i]==&supports[expected[i]]&&supports[i].visibilityMark==scene.visibilityStamp,"original order and marks");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--octree-batch"){Batch();return 0;}
        Tests();std::cout<<"PASS "<<checks<<'/'<<checks<<": Octree frame traversal\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
