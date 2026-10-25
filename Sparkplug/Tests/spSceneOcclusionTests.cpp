#include "Code/Sparkplug/spScene.h"
#include "Code/Sparkplug/spVisibilityManager.h"
#include "Code/Sparkplug/spOcclusionVolume.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spMesh.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/SparkplugPC/spPCPartitionRenderable.h"
#include "Analysis/Host/LegacySortPolicy.h"
#include "Analysis/Host/VisibilityGraphSnapshot.h"
#include "Analysis/PC/spOcclusionSceneMath.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
using Scene=spScene;
using Manager=spVisibilityManager;
using Volume=spOcclusionVolume;
using Status=Scene::OcclusionStatusForAnalysis;
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t Bits(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
float ReadFloat(){std::uint32_t word;Check(bool(std::cin>>word),"short float-word input");float value;std::memcpy(&value,&word,4);return value;}
Scene::OcclusionCameraForAnalysis IdentityCamera()
{Scene::OcclusionCameraForAnalysis camera;camera.view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};return camera;}
class LiteralMesh final:public spMesh
{public:explicit LiteralMesh(const BoundingSphere& sphere){SetBoundingSphereForAnalysis(sphere);}};
void Initialize(Volume& volume)
{
    const std::array<Volume::Vector3,4> points{{{-2,-2,10},{2,-2,10},{2,2,10},{-2,2,10}}};
    spVertexBuffer vb;spIndexBuffer ib;std::vector<std::byte> bytes(48);std::memcpy(bytes.data(),points.data(),48);
    Check(vb.InitializeFromDataForAnalysis(0,4,0,bytes)&&ib.InitializeForAnalysis(2,spIndexBuffer::eIndexBufferType::Type2),"quad CPU buffers");
    const std::uint16_t indices[]{0,1,2,0,2,3};for(unsigned i=0;i<6;++i)Check(ib.SetIndexForAnalysis(i,indices[i]),"quad index");
    Check(volume.InitializeForAnalysis(ib,vb,sparkplug::host::LegacySortPolicy::GeometryDispatch()),"fresh source Init");
}
void World(Volume& volume,const Volume::Vector3& position)
{volume.SetPositionForAnalysis(position);volume.MarkLocalTransformDirtyForAnalysis();Check(volume.UpdateWorldForAnalysis(),"source world update");}
void Collect(Manager& manager,Volume& volume)
{
    // Explicit fixture packing of the CPU-phase input; not a substitute for
    // the original upstream Scene/Visibility traversal.
    const auto mark=volume.GetVisibilityMarkForAnalysis();volume.SetVisibilityMarkForAnalysis(manager.GetFrameStampForAnalysis()-1u);
    const auto result=manager.TrySubmitOccluderForAnalysis(volume,{},true);volume.SetVisibilityMarkForAnalysis(mark);
    Check(result.completed&&result.submitted,"borrowed collected volume input");
}
void Collect(Manager& manager,Manager::SupportForAnalysis& support)
{
    const auto mark=support.visibilityMark;support.visibilityMark=manager.GetFrameStampForAnalysis()-1u;
    const bool accepted=manager.TrySubmitForAnalysis(support,{},true);support.visibilityMark=mark;
    Check(accepted,"borrowed collected support input");
}
template<class T> int Identity(const std::vector<T*>& values,const T* value)
{if(!value)return -1;for(std::size_t i=0;i<values.size();++i)if(values[i]==value)return int(i);throw std::runtime_error("unknown output pointer");}
void CaptureBatch(bool frame=false,bool actualClasses=false)
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=16,"bounded Scene case count");std::cout<<'[';
    for(std::size_t c=0;c<count;++c)
    {
        std::uint32_t sceneStamp,managerStamp;std::size_t volumeCount,supportCount,collectedVolumes,collectedSupports;unsigned orthographic;
        Check(bool(std::cin>>sceneStamp>>managerStamp>>volumeCount>>supportCount>>collectedVolumes>>collectedSupports>>orthographic)&&volumeCount<=32&&supportCount<=64&&collectedVolumes<=32&&collectedSupports<=64&&orthographic<=1,"bounded Scene record");
        unsigned cull=1,debug=0,overrideFlag=0;std::uint32_t rootMark=0;
        if(frame)Check(bool(std::cin>>cull>>debug>>rootMark>>overrideFlag)&&cull<=1&&debug<=1&&overrideFlag<=1,"frame flags");
        Scene::OcclusionCameraForAnalysis camera;for(auto& value:camera.view)value=ReadFloat();camera.orthographic=orthographic!=0;
        Volume::Vector3 position{};Manager::TraversalCameraForAnalysis traversalCamera,overrideCamera;
        const auto readCamera=[](Manager::TraversalCameraForAnalysis& value){
            for(auto& word:value.position)word=ReadFloat();for(auto& word:value.forward)word=ReadFloat();
            for(auto& word:value.farPlane.equation)word=ReadFloat();for(auto& plane:value.sides)for(auto& word:plane.equation)word=ReadFloat();};
        if(frame){readCamera(traversalCamera);if(overrideFlag)readCamera(overrideCamera);}
        else for(auto& value:position)value=ReadFloat();
        std::vector<std::unique_ptr<Volume>> owners;std::vector<Volume*> volumes;
        for(std::size_t i=0;i<volumeCount;++i)
        {
            auto volume=std::make_unique<Volume>();Initialize(*volume);std::uint32_t mark;Check(bool(std::cin>>mark),"volume mark");
            Volume::Vector3 localPosition,scale;Volume::Matrix3 orientation;
            for(auto& value:localPosition)value=ReadFloat();for(auto& value:scale)value=ReadFloat();for(auto& value:orientation)value=ReadFloat();
            volume->SetScaleForAnalysis(scale);volume->SetOrientationForAnalysis(orientation);World(*volume,localPosition);volume->SetVisibilityMarkForAnalysis(mark);
            volumes.push_back(volume.get());owners.push_back(std::move(volume));
        }
        std::vector<Manager::SupportForAnalysis> supports(supportCount);std::vector<Manager::SupportForAnalysis*> supportPointers;
        for(auto& support:supports){support.kind=Manager::SupportKindForAnalysis::RenderNode;Check(bool(std::cin>>support.visibilityMark),"support mark");if(frame){unsigned enabled,bypass;Check(bool(std::cin>>enabled>>bypass)&&enabled<=1&&bypass<=1,"support flags");support.enabled=enabled!=0;support.cullBypass=bypass!=0;}for(auto& value:support.worldSphere)value=ReadFloat();supportPointers.push_back(&support);}
        Manager manager;manager.SetFrameStampForAnalysis(managerStamp);
        auto root=std::make_unique<spPartitionNode>();auto zone=std::make_shared<spZone>();Manager::TraversalNodeForAnalysis node;node.spatial=root.get();node.visibilityMark=rootMark;
        if(frame){root->SetZoneForAnalysis(zone);Check(zone->AppendRootForAnalysis(root.get()),"represented zone root");}
        std::vector<std::unique_ptr<spRenderNode>> renderOwners;
        std::unique_ptr<sparkplug::host::VisibilityGraphSnapshot> snapshot;
        if(actualClasses)
        {
            root->SetVisibilityMarkForAnalysis(rootMark);
            for(const auto& support:supports)
            {
                auto render=std::make_unique<spRenderNode>();auto model=std::make_shared<spModel>();
                model->SetBaseMeshForAnalysis(std::make_shared<LiteralMesh>(support.worldSphere));
                Check(render->AttachRenderableForAnalysis(model),"actual model/support append producer");
                render->MarkLocalTransformDirtyForAnalysis();Check(render->UpdateWorldForAnalysis(),"actual RenderNode world/bounds producer");
                render->SetEnabledForAnalysis(support.enabled,false);render->SetCullBypassForAnalysis(support.cullBypass);render->SetVisibilityMarkForAnalysis(support.visibilityMark);renderOwners.push_back(std::move(render));
            }
        }
        for(std::size_t i=0;i<collectedVolumes;++i){std::size_t id;Check(bool(std::cin>>id)&&id<volumes.size(),"collected volume id");if(actualClasses)Check(root->InsertOccluderForAnalysis(volumes[id]),"actual reciprocal occluder insertion");else if(frame)node.occluders.push_back(volumes[id]);else Collect(manager,*volumes[id]);}
        for(std::size_t i=0;i<collectedSupports;++i){std::size_t id;Check(bool(std::cin>>id)&&id<supports.size(),"collected support id");if(actualClasses)Check(root->InsertDynamicForAnalysis(renderOwners[id].get()),"actual reciprocal dynamic insertion");else if(frame)node.dynamics.push_back(&supports[id]);else Collect(manager,supports[id]);}
        Scene scene{Scene::AnalysisConstruction{}};scene.SetVisibilityStampForAnalysis(sceneStamp);
        Scene::VisibilityOcclusionResultForAnalysis combined;Status result;
        if(frame)
        {
            Manager::TraversalSceneForAnalysis graph;graph.partitionRoot=root.get();graph.nodes={&node};graph.unclippedDebug=debug!=0;if(overrideFlag)graph.overrideCamera=&overrideCamera;
            auto* selectedGraph=&graph;
            if(actualClasses)
            {
                snapshot=std::make_unique<sparkplug::host::VisibilityGraphSnapshot>();Check(snapshot->Build(*root)==sparkplug::host::VisibilityGraphSnapshot::Status::Complete,"actual class graph snapshot");
                snapshot->Scene().unclippedDebug=debug!=0;if(overrideFlag)snapshot->Scene().overrideCamera=&overrideCamera;selectedGraph=&snapshot->Scene();
            }
            manager.SetSphereCullingForAnalysis(cull!=0);combined=scene.ProcessVisibilityOcclusionForAnalysis(manager,*selectedGraph,&traversalCamera,camera,{});result=combined.occlusion;
            if(snapshot)Check(snapshot->PublishMarks(),"actual class mark publication");
            if(c)std::cout<<',';std::cout<<"{\"completed\":"<<combined.completed<<",\"traversal\":"<<unsigned(combined.traversal)<<",\"occlusion\":"<<unsigned(combined.occlusion)<<",\"sceneStamp\":"<<scene.GetVisibilityStampForAnalysis()<<",\"frameStamp\":"<<manager.GetFrameStampForAnalysis()<<",\"nodeMarks\":["<<(actualClasses?root->GetVisibilityMarkForAnalysis():node.visibilityMark)<<"],\"phase\":";
        }
        else{result=scene.ProcessOcclusionForAnalysis(manager,camera,position,{});if(c)std::cout<<',';}
        std::cout<<"{\"status\":"<<unsigned(result)<<",\"queue\":[";bool first=true;
        for(const auto& entry:scene.GetOcclusionEntriesForAnalysis()){if(!first)std::cout<<',';first=false;std::cout<<'['<<Identity(volumes,entry.volume)<<','<<Bits(entry.orderingKey)<<']';}
        std::cout<<"],\"visible\":[";first=true;for(auto* support:manager.GetVisibleForAnalysis())
        {
            if(!first)std::cout<<',';first=false;
            if(!snapshot)std::cout<<Identity(supportPointers,support);
            else if(!support)std::cout<<-1;
            else{auto* source=snapshot->SourceOf(support);std::size_t id=0;while(id<renderOwners.size()&&renderOwners[id].get()!=source)++id;Check(id<renderOwners.size(),"actual visible object identity");std::cout<<id;}
        }
        std::cout<<"],\"volumeMarks\":[";first=true;for(auto* volume:volumes){if(!first)std::cout<<',';first=false;std::cout<<volume->GetVisibilityMarkForAnalysis();}
        std::cout<<"],\"supportMarks\":[";for(std::size_t i=0;i<supports.size();++i){if(i)std::cout<<',';std::cout<<(actualClasses?renderOwners[i]->GetVisibilityMarkForAnalysis():supports[i].visibilityMark);}
        std::cout<<"],\"cache\":[";first=true;for(auto* volume:volumes)
        {
            if(!first)std::cout<<',';first=false;const auto& planes=volume->GetOcclusionPlanesForAnalysis();
            std::cout<<"{\"dirty\":"<<volume->IsCameraDirtyForAnalysis()<<",\"walkStamp\":"<<volume->GetCameraWalkStampForAnalysis()<<",\"planes\":[";bool firstPlane=true;
            for(const auto& plane:planes.planes){if(!firstPlane)std::cout<<',';firstPlane=false;std::cout<<'[';for(unsigned i=0;i<4;++i){if(i)std::cout<<',';std::cout<<Bits(plane.equation[i]);}std::cout<<','<<plane.enabled<<']';}
            std::cout<<"],\"active\":"<<planes.activeCount<<'}';
        }
        std::cout<<"]}";if(frame)std::cout<<'}';
    }
    std::cout<<"]\n";
}
void Directed()
{
    Scene scene{Scene::AnalysisConstruction{}};Check(scene.IsKindOf(spNamedObject::ClassID)&&scene.StaticRTTI().factory==nullptr,"partial Scene physical Named prefix, no factory substitution");
    spCloneManager clone;Check(!scene.vfunc_10(clone),"unavailable native Scene clone");
    auto camera=IdentityCamera();float key=-1;
    using namespace sparkplug::evidence::pc::occlusion_scene_math;
    Check(OrderingKey({3,4,12},camera.view,false,key)&&key==169,"perspective key");Check(OrderingKey({3,4,12},camera.view,true,key)&&key==144,"orthographic key");
    auto extended=camera;extended.view={};extended.view[2]=16777216;extended.view[6]=1;
    Check(OrderingKey({1,1,1},extended.view,true,key)&&Bits(key)==0x57800001,"Z stays extended through square");
    const auto square=SquareNearest({std::numeric_limits<std::uint64_t>::max(),-63,false});
    Check(square.mantissa==std::numeric_limits<std::uint64_t>::max()-1&&square.exponent==-62&&!square.negative,"full 128-bit square rounds once");
    extended.view[0]=std::numeric_limits<float>::quiet_NaN();key=17;Check(!OrderingKey({1,1,1},extended.view,false,key)&&key==17,"numeric refusal preserves output");
    Manager empty;Check(scene.ProcessOcclusionForAnalysis(empty,extended,{0,0,0},{})==Status::Complete,"empty input does not consume camera math");
    Volume nearVolume,farVolume;Initialize(nearVolume);Initialize(farVolume);World(nearVolume,{0,0,0});World(farVolume,{0,0,2});
    Manager manager;manager.SetFrameStampForAnalysis(1);Collect(manager,nearVolume);Collect(manager,farVolume);
    std::array<Manager::SupportForAnalysis,3> supports;
    supports[0].worldSphere={0,0,20,1};supports[1].worldSphere={0,0,5,1};supports[2].worldSphere={8,0,20,1};
    for(auto& support:supports){support.visibilityMark=1;Collect(manager,support);}
    scene.SetVisibilityStampForAnalysis(1);Check(scene.ProcessOcclusionForAnalysis(manager,camera,{0,0,0},{})==Status::Complete,"concrete Scene CPU phase");
    const auto& entries=scene.GetOcclusionEntriesForAnalysis();Check(entries.size()==2&&entries[0].volume==&nearVolume&&entries[0].orderingKey==100&&entries[1].volume==nullptr&&entries[1].orderingKey==144,"ordering and later-volume null hole");
    const auto& visible=manager.GetVisibleForAnalysis();Check(visible.size()==3&&!visible[0]&&visible[1]==&supports[1]&&visible[2]==&supports[2],"strict support occlusion retains logical slots");
    Check(supports[0].visibilityMark==0&&supports[1].visibilityMark==1&&supports[2].visibilityMark==1,"only hidden support mark rolls back");
    Check(!manager.HideVisibleSupportForAnalysis(99,0)&&!manager.HideVisibleSupportForAnalysis(0,0),"borrowed mutation refuses absent slots");
    Volume equalFirst,equalSecond;Initialize(equalFirst);Initialize(equalSecond);World(equalFirst,{0,0,0});World(equalSecond,{0,0,0});
    Manager equal;equal.SetFrameStampForAnalysis(1);Collect(equal,equalSecond);Collect(equal,equalFirst);
    Check(scene.ProcessOcclusionForAnalysis(equal,camera,{0,0,0},{})==Status::Complete&&scene.GetOcclusionEntriesForAnalysis()[0].volume==&equalSecond&&scene.GetOcclusionEntriesForAnalysis()[1].volume==&equalFirst,"finite equal-key insertion is stable; tangent shape remains");
    std::vector<std::unique_ptr<Volume>> largeOwners;Manager large;large.SetFrameStampForAnalysis(1);
    for(unsigned i=0;i<33;++i){auto volume=std::make_unique<Volume>();Collect(large,*volume);largeOwners.push_back(std::move(volume));}
    Check(scene.ProcessOcclusionForAnalysis(large,camera,{0,0,0},{})==Status::MissingOrderingPolicy,"large native introsort remains an explicit boundary");
    Check(scene.ProcessOcclusionForAnalysis(large,camera,{0,0,0},sparkplug::host::LegacySortPolicy::OcclusionDispatch())==Status::Complete,"explicit versioned host policy preserves records");
    Scene::OcclusionSortForAnalysis corrupt{nullptr,[](void*,Scene::OcclusionEntryForAnalysis* values,std::size_t){values[0].volume=nullptr;return true;}};
    Check(scene.ProcessOcclusionForAnalysis(large,camera,{0,0,0},corrupt)==Status::InvalidOrderingPolicy,"host policy cannot fabricate record loss");
    Manager combinedManager;combinedManager.SetFrameStampForAnalysis(9);scene.SetVisibilityStampForAnalysis(7);
    auto root=std::make_unique<spPartitionNode>();auto zone=std::make_shared<spZone>();root->SetZoneForAnalysis(zone);Check(zone->AppendRootForAnalysis(root.get()),"composed frame root");
    Manager::TraversalNodeForAnalysis node;node.spatial=root.get();node.occluders={&nearVolume,&farVolume};for(auto& support:supports){support.kind=Manager::SupportKindForAnalysis::RenderNode;node.dynamics.push_back(&support);}
    Manager::TraversalSceneForAnalysis graph;graph.partitionRoot=root.get();graph.nodes={&node};graph.unclippedDebug=true;
    Manager::TraversalCameraForAnalysis passed,overrideCamera;overrideCamera.position={0,0,1};graph.overrideCamera=&overrideCamera;
    const auto combined=scene.ProcessVisibilityOcclusionForAnalysis(combinedManager,graph,&passed,camera,{});
    Check(combined.completed&&combined.traversal==Manager::TraversalStatusForAnalysis::Complete&&combined.occlusion==Status::Complete,"actual traversal then concrete Scene occlusion");
    Check(scene.GetVisibilityStampForAnalysis()==10&&graph.visibilityStamp==10&&combinedManager.GetFrameStampForAnalysis()==10,"real frame publishes Scene stamp before support removal");
    Check(nearVolume.GetCachedCameraForAnalysis()==overrideCamera.position&&combinedManager.GetVisibleForAnalysis().size()==3&&!combinedManager.GetVisibleForAnalysis()[0]&&supports[0].visibilityMark==9,"override drives camera geometry; hidden stamp is newSceneStamp minusone");
    Manager::TraversalSceneForAnalysis missingRoot;const auto oldStamp=scene.GetVisibilityStampForAnalysis();
    Check(!scene.ProcessVisibilityOcclusionForAnalysis(combinedManager,missingRoot,&passed,camera,{}).completed&&scene.GetVisibilityStampForAnalysis()==oldStamp,"non-partition Scene branch is explicitly unavailable");
    spRenderNode actualNode;auto model=std::make_shared<spModel>();model->SetBaseMeshForAnalysis(std::make_shared<LiteralMesh>(spMesh::BoundingSphere{0,0,20,1}));
    Check(actualNode.AttachRenderableForAnalysis(model),"actual adapter model producer");actualNode.MarkLocalTransformDirtyForAnalysis();Check(actualNode.UpdateWorldForAnalysis(),"actual adapter world producer");
    Check(root->InsertDynamicForAnalysis(&actualNode)&&root->InsertDynamicForAnalysis(&actualNode),"actual adapter preserves duplicated memberships");
    auto actualStatic=std::make_shared<spStaticRenderObject>();Check(actualStatic->AttachRenderableForAnalysis(model)&&root->InsertStaticForAnalysis(actualStatic),"actual static support producer and ownership");
    auto payload=std::make_unique<spPCPartitionRenderable>();auto* payloadIdentity=payload.get();Check(payload->AttachRenderableForAnalysis(model)&&root->SetPartitionRenderableForAnalysis(std::move(payload)),"actual owned payload producer");
    sparkplug::host::VisibilityGraphSnapshot owned;Check(owned.Build(*root)==sparkplug::host::VisibilityGraphSnapshot::Status::Complete,"snapshot from class-owned storage");
    Check(owned.Scene().nodes.size()==1&&owned.Scene().nodes[0]->dynamics.size()==2&&owned.Scene().nodes[0]->dynamics[0]==owned.Scene().nodes[0]->dynamics[1],"repeated physical support identity uses one snapshot record");
    owned.Scene().unclippedDebug=true;Manager ownManager;const auto ownResult=scene.ProcessVisibilityOcclusionForAnalysis(ownManager,owned.Scene(),&passed,camera,{});
    Check(ownResult.completed&&owned.PublishMarks()&&ownManager.GetVisibleForAnalysis().size()==3,"concrete traversal deduplicates actual support identities");
    const auto& actualVisible=ownManager.GetVisibleForAnalysis();Check(owned.SourceOf(actualVisible[0])==payloadIdentity&&owned.SourceOf(actualVisible[1])==actualStatic.get()&&owned.SourceOf(actualVisible[2])==&actualNode,"actual payload-static-dynamic order");
    Check(actualNode.GetVisibilityMarkForAnalysis()==scene.GetVisibilityStampForAnalysis()&&actualStatic->GetVisibilityMarkForAnalysis()==scene.GetVisibilityStampForAnalysis()&&payloadIdentity->GetVisibilityMarkForAnalysis()==scene.GetVisibilityStampForAnalysis(),"all actual source support kinds receive published frame marks");
    Check(owned.Build(*root)==sparkplug::host::VisibilityGraphSnapshot::Status::AlreadyAttempted,"snapshot input/output lifetime cannot be silently rebuilt");
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--scene-batch"){CaptureBatch();return 0;}
    if(argc==2&&std::string(argv[1])=="--scene-frame-batch"){CaptureBatch(true);return 0;}
    if(argc==2&&std::string(argv[1])=="--scene-owned-frame-batch"){CaptureBatch(true,true);return 0;}
    Directed();std::cout<<"PASS "<<checks<<'/'<<checks<<": Scene CPU occlusion phase\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
