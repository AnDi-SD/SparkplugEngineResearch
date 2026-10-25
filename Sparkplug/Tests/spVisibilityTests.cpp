#include "Code/Sparkplug/spVisibilityManager.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spZonePortal.h"
#include "Analysis/PC/SparkplugAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace vm = sparkplug::evidence::pc::visibility_math;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    float ReadFloat()
    {
        std::uint32_t bits;
        if (!(std::cin >> bits))
            throw std::runtime_error("incomplete float bits");
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    vm::PlaneSet ReadPlanes(std::uint32_t count, std::uint32_t active)
    {
        if (count > 16)
            throw std::runtime_error("bounded planes");
        vm::PlaneSet result;
        result.activeCount = active;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            vm::Plane plane;
            for (auto& value : plane.equation)
                value = ReadFloat();
            unsigned enabled;
            std::cin >> enabled;
            plane.enabled = enabled != 0;
            result.planes.push_back(plane);
        }
        return result;
    }
    void MathBatch()
    {
        std::uint32_t count, active;
        while (std::cin >> count >> active)
        {
            vm::Sphere sphere;
            for (auto& value : sphere)
                value = ReadFloat();
            auto planes = ReadPlanes(count, active);
            std::cout << vm::IsOutside(planes, sphere) << ','
                      << static_cast<unsigned>(vm::Classify(planes, sphere)) << ','
                      << vm::IsFullyInside(planes, sphere) << '\n';
        }
    }
    void SelectBatch()
    {
        using Manager = spVisibilityManager;
        std::uint32_t seed, culling, debug;
        while (std::cin >> seed >> culling >> debug)
        {
            auto planes = ReadPlanes(6, 6);
            Manager manager;
            manager.SetFrameStampForAnalysis(seed);
            manager.SetSphereCullingForAnalysis(culling != 0);
            std::array<Manager::SupportForAnalysis, 6> objects;
            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                auto& obj = objects[i];
                obj.kind = i == 0  ? Manager::SupportKindForAnalysis::Partition
                           : i < 3 ? Manager::SupportKindForAnalysis::Static
                                   : Manager::SupportKindForAnalysis::RenderNode;
                unsigned enabled, bypass;
                std::cin >> obj.visibilityMark >> enabled >> bypass;
                obj.enabled = enabled != 0;
                obj.cullBypass = bypass != 0;
                for (auto& value : obj.worldSphere)
                    value = ReadFloat();
            }
            std::uint32_t sceneStamp;
            manager.BeginFrameForAnalysis(sceneStamp);
            // Explicit original list order with duplicate static/dynamic refs.
            for (auto i : {0, 1, 2, 1, 3, 4, 5, 3})
                (void)manager.TrySubmitForAnalysis(objects[i], planes, debug != 0);
            std::cout << sceneStamp << ',' << manager.GetVisibleForAnalysis().size();
            for (const auto* object : manager.GetVisibleForAnalysis())
                std::cout << ',' << object - objects.data();
            for (const auto& object : objects)
                std::cout << ',' << object.visibilityMark;
            std::cout << '\n';
        }
    }
    void UnitTests()
    {
        vm::PlaneSet planes{{{{{0, 0, 1, 0}}, true}}, 1};
        Check(!vm::IsOutside(planes, {0, 0, -1, 1}), "negative tangent retained");
        Check(vm::IsOutside(planes, {0, 0, -2, 1}), "strict outside");
        Check(vm::Classify(planes, {0, 0, 2, 1}) == vm::SphereClass::Inside, "inside");
        Check(vm::Classify(planes, {0, 0, 1, 1}) == vm::SphereClass::Intersecting,
              "positive tangent");
        Check(vm::Classify(planes, {0, 0, -1, 1}) == vm::SphereClass::Intersecting,
              "negative tangent");
        planes.activeCount = 0;
        Check(!vm::IsOutside(planes, {0, 0, -2, 1}), "cached-count gate");
        Check(vm::Classify(planes, {0, 0, -2, 1}) == vm::SphereClass::Outside,
              "classifier ignores count");
        planes.activeCount = 1;
        const vm::Sphere nan{0, 0, std::numeric_limits<float>::quiet_NaN(), 1};
        Check(!vm::IsOutside(planes, nan), "unordered not outside");
        Check(vm::Classify(planes, nan) == vm::SphereClass::Intersecting, "unordered intersecting");
        Check(vm::IsFullyInside(planes, {0, 0, 2, 1}), "strictly fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, 1, 1}), "positive tangent not fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, -1, 1}), "negative tangent not fully inside");
        Check(!vm::IsFullyInside(planes, nan), "unordered not fully inside");
        Check(!vm::IsFullyInside(planes, {0, 0, 0, -1}),
              "negative radius first rejection retained");
        planes.activeCount = 0;
        Check(vm::IsFullyInside(planes, {0, 0, -2, 1}), "empty cached count fully inside");
        planes.activeCount = 1;
        planes.planes[0].enabled = false;
        Check(vm::IsFullyInside(planes, {0, 0, -2, 1}),
              "disabled plane ignored for full inclusion");
        planes.planes[0].enabled = true;
        spVisibilityManager manager;
        spVisibilityManager::SupportForAnalysis object;
        object.worldSphere = {0, 0, 2, 1};
        std::uint32_t sceneStamp = 42;
        manager.BeginFrameForAnalysis(sceneStamp);
        Check(sceneStamp == 1 && manager.GetFrameStampForAnalysis() == 1, "frame publish");
        Check(manager.TrySubmitForAnalysis(object, planes), "submit");
        Check(!manager.TrySubmitForAnalysis(object, planes), "deduplicate");
        manager.SetFrameStampForAnalysis(0xffffffffu);
        object.visibilityMark = 0;
        manager.BeginFrameForAnalysis(sceneStamp);
        Check(sceneStamp == 0 && !manager.TrySubmitForAnalysis(object, planes), "native zero wrap");
        manager.BeginFrameForAnalysis(sceneStamp);
        object.kind = spVisibilityManager::SupportKindForAnalysis::RenderNode;
        object.enabled = false;
        Check(!manager.TrySubmitForAnalysis(object, planes), "disabled dynamic");
        Check(manager.TrySubmitForAnalysis(object, planes, true), "debug bypasses disabled");
        auto clone = manager.Clone();
        auto* typed = dynamic_cast<spVisibilityManager*>(clone.get());
        Check(typed && typed->GetFrameStampForAnalysis() == 0 &&
                  typed->GetVisibleForAnalysis().empty(),
              "inherited-only clone fresh own state");
        Check(manager.IsExactly(spVisibilityManager::ClassID) &&
                  manager.IsKindOf(spBaseObject::ClassID),
              "original RTTI identity");
    }
    void GraphBatch()
    {
        using M=spVisibilityManager;
        std::uint32_t seed,culling,debug,rootCount,present,word10;
        while(std::cin>>seed>>culling>>debug>>rootCount>>present>>word10)
        {
            Check(rootCount<=2,"bounded root count");
            M::TraversalCameraForAnalysis camera;
            for(auto& value:camera.position)value=ReadFloat();
            for(auto& value:camera.forward)value=ReadFloat();
            for(auto& value:camera.farPlane.equation)value=ReadFloat();
            for(auto& plane:camera.sides)for(auto& value:plane.equation)value=ReadFloat();
            std::array<M::SupportForAnalysis,7> objects;
            for(unsigned i=0;i<objects.size();++i)
            {
                auto& object=objects[i];
                object.kind=i==0?M::SupportKindForAnalysis::Partition:
                    (i<3||i==6?M::SupportKindForAnalysis::Static:M::SupportKindForAnalysis::RenderNode);
                unsigned enabled,bypass;
                std::cin>>object.visibilityMark>>enabled>>bypass;
                object.enabled=enabled!=0;object.cullBypass=bypass!=0;
                for(auto& value:object.worldSphere)value=ReadFloat();
            }
            auto root=std::make_unique<spPartitionNode>(),second=std::make_unique<spPartitionNode>();
            auto zone=std::make_shared<spZone>();root->SetZoneForAnalysis(zone);
            if(rootCount)(void)zone->AppendRootForAnalysis(root.get());
            if(rootCount==2)(void)zone->AppendRootForAnalysis(second.get());
            M::TraversalNodeForAnalysis firstView,secondView;
            firstView.spatial=root.get();firstView.payload=&objects[0];firstView.visibilityMark=31;
            firstView.statics={&objects[1],&objects[2],&objects[1]};
            firstView.dynamics={&objects[3],&objects[4],&objects[5],&objects[3]};
            secondView.spatial=second.get();secondView.visibilityMark=37;secondView.statics={&objects[6]};
            M manager;manager.SetFrameStampForAnalysis(seed);manager.SetSphereCullingForAnalysis(culling!=0);
            M::TraversalSceneForAnalysis scene;
            scene.partitionRoot=present?root.get():nullptr;scene.nodes={&firstView,&secondView};
            scene.unclippedDebug=debug!=0;scene.opaqueManagerWord10=word10;
            const auto status=manager.ProcessSceneForAnalysis(scene,&camera);
            std::cout<<static_cast<unsigned>(status)<<','<<scene.visibilityStamp<<','<<scene.opaqueManagerWord10
                <<','<<firstView.visibilityMark<<','<<secondView.visibilityMark<<','<<manager.GetVisibleForAnalysis().size();
            for(const auto* object:manager.GetVisibleForAnalysis())std::cout<<','<<object-objects.data();
            for(const auto& object:objects)std::cout<<','<<object.visibilityMark;
            std::cout<<'\n';
        }
    }
    void GraphTests()
    {
        using M=spVisibilityManager;using S=M::TraversalStatusForAnalysis;
        M manager;M::TraversalSceneForAnalysis absent;
        absent.nodes={nullptr};absent.opaqueManagerWord10=0xA5;
        Check(manager.ProcessSceneForAnalysis(absent,nullptr)==S::Complete&&absent.visibilityStamp==1
            &&absent.opaqueManagerWord10==0xA5,"no partition publishes stamp without reading camera/views or word10");
        auto root=std::make_unique<spPartitionNode>(),second=std::make_unique<spPartitionNode>();
        auto zone=std::make_shared<spZone>();root->SetZoneForAnalysis(zone);
        Check(zone->AppendRootForAnalysis(root.get())&&zone->AppendRootForAnalysis(second.get()),"two borrowed zone roots");
        M::TraversalNodeForAnalysis a,b;a.spatial=root.get();b.spatial=second.get();b.visibilityMark=37;
        M::SupportForAnalysis first,last;first.kind=last.kind=M::SupportKindForAnalysis::Static;
        first.worldSphere=last.worldSphere={0,0,10,1};a.statics={&first,&first};b.statics={&last};
        M::TraversalCameraForAnalysis camera;camera.farPlane.equation={0,0,-1,-1000};
        for(auto& side:camera.sides)side.equation={0,0,0,-1000};
        M::TraversalSceneForAnalysis scene;scene.partitionRoot=root.get();scene.nodes={&a,&b};scene.opaqueManagerWord10=0xA5;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&manager.GetVisibleForAnalysis().size()==2
            &&a.visibilityMark==2&&b.visibilityMark==2&&scene.opaqueManagerWord10==0xA5,"normal visits zone roots and preserves secondary manager word");
        b.visibilityMark=37;scene.unclippedDebug=true;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&manager.GetVisibleForAnalysis().size()==1
            &&b.visibilityMark==37,"Debug loop repeats selected system root instead of each zone root");
        scene.unclippedDebug=false;first.worldSphere={0,0,-2,0.25f};last.worldSphere={0,0,-20,0.25f};
        auto overrideCamera=camera;overrideCamera.position[2]=-10;scene.overrideCamera=&overrideCamera;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&manager.GetVisibleForAnalysis().size()==1
            &&manager.GetVisibleForAnalysis()[0]==&first,"override camera moves first halfspace through its position");
        scene.overrideCamera=nullptr;const auto before=manager.GetFrameStampForAnalysis();
        a.hasOccluders=true;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::UnsupportedOccluder
            &&manager.GetFrameStampForAnalysis()==before,"unrepresented occlusion is explicit refusal before mutation");
        a.hasOccluders=false;auto closed=std::make_shared<spZonePortal>();closed->SetOpenForAnalysis(false);
        closed->SetVisibilityMarkForAnalysis(77);Check(root->AppendPortalForAnalysis(closed),"closed portal setup");
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&closed->GetVisibilityMarkForAnalysis()==77,
            "closed portal skips before geometry/stamp/destination access");

        auto octree=std::make_unique<spOctreeNode>();auto octreeZone=std::make_shared<spZone>();
        Check(octree->SetGeometryForAnalysis({0,0,0},{-8,-8,-8},{8,8,8}),"octree geometry");
        octree->SetZoneForAnalysis(octreeZone);Check(octreeZone->AppendRootForAnalysis(octree.get()),"octree zone root");
        std::array<M::TraversalNodeForAnalysis,9> nodes;nodes[0].spatial=octree.get();
        std::array<M::SupportForAnalysis,8> disabled;
        M::TraversalSceneForAnalysis octreeScene;octreeScene.partitionRoot=octree.get();octreeScene.nodes.push_back(&nodes[0]);
        for(unsigned i=0;i<8;++i)
        {
            auto child=std::make_unique<spPartitionNode>();nodes[i+1].spatial=child.get();
            Check(octree->SetChildForAnalysis(i,std::move(child)),"owned octree child");
            disabled[i].kind=M::SupportKindForAnalysis::RenderNode;disabled[i].enabled=false;
            disabled[i].worldSphere={0,0,0,1};nodes[i+1].dynamics={&disabled[i]};octreeScene.nodes.push_back(&nodes[i+1]);
        }
        camera.position={0,0,-32};
        Check(manager.ProcessSceneForAnalysis(octreeScene,&camera)==S::Complete&&manager.GetVisibleForAnalysis().size()==8,
            "zero reduced planes uses unclipped child traversal including disabled dynamics");
        auto deep=std::make_unique<spOctreeNode>();auto* cursor=deep.get();
        std::array<M::TraversalNodeForAnalysis,130> deepViews;
        M::TraversalSceneForAnalysis deepScene;deepScene.partitionRoot=deep.get();
        for(unsigned i=0;i<deepViews.size();++i)
        {
            Check(cursor->SetGeometryForAnalysis({0,0,0},{-8,-8,-8},{8,8,8}),"deep query geometry");
            deepViews[i].spatial=cursor;deepScene.nodes.push_back(&deepViews[i]);
            if(i+1<deepViews.size())
            {
                auto next=std::make_unique<spOctreeNode>();auto* raw=next.get();
                Check(cursor->SetChildForAnalysis(0,std::move(next)),"deep query child");cursor=raw;
            }
        }
        camera.position={-1,-1,-1};const auto previousStamp=manager.GetFrameStampForAnalysis();
        Check(manager.ProcessSceneForAnalysis(deepScene,&camera)==S::HostTraversalLimit
            &&manager.GetFrameStampForAnalysis()==previousStamp,"FindLeaf depth guarded before recursive query and frame mutation");
    }
    void PortalTests()
    {
        using M=spVisibilityManager;using S=M::TraversalStatusForAnalysis;
        auto a=std::make_unique<spPartitionNode>(),b=std::make_unique<spPartitionNode>();
        auto za=std::make_shared<spZone>(),zb=std::make_shared<spZone>();
        a->SetZoneForAnalysis(za);b->SetZoneForAnalysis(zb);
        Check(za->AppendRootForAnalysis(a.get())&&zb->AppendRootForAnalysis(b.get()),"portal zone roots");
        auto portal=std::make_shared<spZonePortal>();portal->SetDestinationForAnalysis(zb.get());
        Check(portal->SetPolygonForAnalysis({{-2,-2,10},{2,-2,10},{2,2,10},{-2,2,10}})
            &&a->AppendPortalForAnalysis(portal),"portal concrete polygon and membership");
        M::TraversalNodeForAnalysis av,bv;av.spatial=a.get();bv.spatial=b.get();
        M::SupportForAnalysis inside,side,before;
        inside.worldSphere={0,0,20,.25f};side.worldSphere={8,0,20,.25f};before.worldSphere={0,0,5,.25f};
        bv.statics={&inside,&side,&before};
        M::TraversalCameraForAnalysis camera;camera.farPlane.equation={0,0,-1,-100};
        for(auto& plane:camera.sides)plane.equation={0,0,0,-1000};
        M manager;M::TraversalSceneForAnalysis scene;scene.partitionRoot=a.get();scene.nodes={&av,&bv};
        std::vector<M::TraversalVisitForAnalysis> trace;scene.trace=&trace;
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&manager.GetVisibleForAnalysis().size()==2
            &&manager.GetVisibleForAnalysis()[0]==&inside&&manager.GetVisibleForAnalysis()[1]==&before,
            "far portal creates concrete aperture, keeping camera near instead of portal plane");
        Check(trace.size()==2&&trace[1].planes.planes.size()==6&&trace[1].incoming==std::array<float,3>{},"far portal trace contains actual new planes and original offset");
        auto back=std::make_shared<spZonePortal>();back->SetDestinationForAnalysis(za.get());
        Check(back->SetPolygonForAnalysis(portal->GetPolygonForAnalysis())&&b->AppendPortalForAnalysis(back),"directed cycle setup");
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&trace.size()==3
            &&trace[2].spatial==a.get(),"portal stamps permit A->B->A root revisit and terminate cycle");
        portal->SetOpenForAnalysis(false);const auto stamp=portal->GetVisibilityMarkForAnalysis();
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&trace.size()==1
            &&portal->GetVisibilityMarkForAnalysis()==stamp,"closed portal keeps stamp");
        portal->SetOpenForAnalysis(true);back->SetOpenForAnalysis(false);camera.position={0,0,10};
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&trace.size()==2
            &&trace[1].incoming[2]==float(10.-double(.002f))&&trace[1].planes.planes.size()==6,
            "epsilon band copies all planes and passes original camera displacement expression");
        camera.position={3,0,10};
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&trace.size()==1,"band edge test rejects outside projected polygon");
        camera.position={0,0,0};scene.unclippedDebug=true;const auto prior=portal->GetVisibilityMarkForAnalysis();
        Check(manager.ProcessSceneForAnalysis(scene,&camera)==S::Complete&&trace.size()==1
            &&trace[0].unclipped&&portal->GetVisibilityMarkForAnalysis()==prior,"unclipped traversal omits portals");
    }
    void PortalBatch()
    {
        using M=spVisibilityManager;
        std::uint32_t seed,culling,debug,word10;
        Check(bool(std::cin>>seed>>culling>>debug>>word10),"portal input header");
        M::TraversalCameraForAnalysis camera;
        for(float& f:camera.position)f=ReadFloat();for(float& f:camera.forward)f=ReadFloat();
        for(float& f:camera.farPlane.equation)f=ReadFloat();
        for(auto& plane:camera.sides)for(float& f:plane.equation)f=ReadFloat();
        std::uint32_t count;std::cin>>count;Check(count==3,"three explicit portal roots");
        std::vector<std::unique_ptr<spPartitionNode>> nodes;std::vector<std::shared_ptr<spZone>> zones;
        std::vector<M::TraversalNodeForAnalysis> views(count);
        for(std::uint32_t i=0;i<count;++i){nodes.push_back(std::make_unique<spPartitionNode>());zones.push_back(std::make_shared<spZone>());
            nodes.back()->SetZoneForAnalysis(zones.back());views[i].spatial=nodes.back().get();std::cin>>views[i].visibilityMark;}
        for(std::uint32_t i=0;i<count;++i){unsigned roots;std::cin>>roots;Check(roots<=4,"bounded zone roots");
            while(roots--){unsigned id;std::cin>>id;Check(id<count&&zones[i]->AppendRootForAnalysis(nodes[id].get()),"borrowed destination root");}}
        std::uint32_t supportCount;std::cin>>supportCount;Check(supportCount<=16,"bounded portal supports");
        std::vector<M::SupportForAnalysis> supports(supportCount);
        for(auto& support:supports){unsigned owner,kind,enabled,bypass;std::cin>>owner>>kind>>support.visibilityMark>>enabled>>bypass;
            Check(owner<count&&kind==2,"directed RenderNode support kind");support.kind=M::SupportKindForAnalysis::RenderNode;
            support.enabled=enabled!=0;support.cullBypass=bypass!=0;for(float& f:support.worldSphere)f=ReadFloat();views[owner].dynamics.push_back(&support);}
        std::uint32_t portalCount;std::cin>>portalCount;Check(portalCount<=8,"bounded portals");
        std::vector<std::shared_ptr<spZonePortal>> portals;
        for(std::uint32_t i=0;i<portalCount;++i){unsigned owner,destination,open,stamp,vertices;std::cin>>owner>>destination>>open>>stamp>>vertices;
            Check(owner<count&&destination<count&&vertices>=3&&vertices<=127,"bounded portal relation");
            std::array<float,4> expected;for(float& f:expected)f=ReadFloat();
            std::vector<spZonePortal::Vector3> points(vertices);for(auto& point:points)for(float& f:point)f=ReadFloat();
            auto portal=std::make_shared<spZonePortal>();Check(portal->SetPolygonForAnalysis(points),"portal authored polygon");
            Check(portal->GetPlaneForAnalysis()==expected,"same original authored plane inputs");
            portal->SetDestinationForAnalysis(zones[destination].get());portal->SetOpenByteForAnalysis(std::uint8_t(open));
            portal->SetVisibilityMarkForAnalysis(stamp);Check(nodes[owner]->AppendPortalForAnalysis(portal),"portal member");portals.push_back(std::move(portal));}
        Check(bool(std::cin),"complete portal input");
        M manager;manager.SetFrameStampForAnalysis(seed);manager.SetSphereCullingForAnalysis(culling!=0);
        M::TraversalSceneForAnalysis scene;scene.partitionRoot=nodes[0].get();scene.opaqueManagerWord10=word10;scene.unclippedDebug=debug!=0;
        for(auto& view:views)scene.nodes.push_back(&view);std::vector<M::TraversalVisitForAnalysis> trace;scene.trace=&trace;
        const auto status=manager.ProcessSceneForAnalysis(scene,&camera);
        const auto bits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
        const auto array=[](const auto& values){std::cout<<'[';bool first=true;for(auto value:values){std::cout<<(first?"":",")<<std::uint32_t(value);first=false;}std::cout<<']';};
        const auto id=[&](const spPartitionNode* p){for(unsigned i=0;i<nodes.size();++i)if(nodes[i].get()==p)return i;throw std::runtime_error("unknown traced root");};
        std::vector<std::uint32_t> nodeMarks,portalMarks,supportMarks,visible;
        for(const auto& view:views)nodeMarks.push_back(view.visibilityMark);for(const auto& portal:portals)portalMarks.push_back(portal->GetVisibilityMarkForAnalysis());
        for(const auto& support:supports)supportMarks.push_back(support.visibilityMark);
        for(auto* support:manager.GetVisibleForAnalysis())visible.push_back(std::uint32_t(support-supports.data()));
        std::cout<<"{\"status\":"<<unsigned(status)<<",\"stamp\":"<<scene.visibilityStamp<<",\"word10\":"<<scene.opaqueManagerWord10<<",\"nodeMarks\":";array(nodeMarks);
        std::cout<<",\"portalMarks\":";array(portalMarks);std::cout<<",\"supportMarks\":";array(supportMarks);std::cout<<",\"visible\":";array(visible);
        std::cout<<",\"trace\":[";bool first=true;for(const auto& entry:trace){std::cout<<(first?"":",")<<"{\"node\":"<<id(entry.spatial)<<",\"unclipped\":"<<(entry.unclipped?"true":"false")<<",\"incoming\":";first=false;
            std::array<std::uint32_t,3> incoming{};for(unsigned i=0;i<3;++i)incoming[i]=bits(entry.incoming[i]);array(incoming);
            std::cout<<",\"active\":"<<entry.planes.activeCount<<",\"planes\":[";bool next=false;for(const auto& plane:entry.planes.planes){std::cout<<(next?",":"");next=true;
                std::array<std::uint32_t,5> words{};for(unsigned i=0;i<4;++i)words[i]=bits(plane.equation[i]);words[4]=plane.enabled?1u:0u;array(words);}std::cout<<"]}";}std::cout<<"]}\n";
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--math-batch")
            MathBatch();
        else if (argc == 2 && std::string(argv[1]) == "--select-batch")
            SelectBatch();
        else if (argc == 2 && std::string(argv[1]) == "--graph-batch")
            GraphBatch();
        else if (argc == 2 && std::string(argv[1]) == "--portal-batch")
            PortalBatch();
        else
        {
            UnitTests();
            GraphTests();
            PortalTests();
            std::cout << "PASS " << checks << '/' << checks << ": visibility slice\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
