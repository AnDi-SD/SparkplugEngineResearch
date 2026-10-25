#include "Code/Sparkplug/spVisibilityManager.h"
#include "Code/Sparkplug/spOcclusionVolume.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spZone.h"
#include "Analysis/Host/LegacySortPolicy.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
using Manager=spVisibilityManager;
using Volume=spOcclusionVolume;
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
float ReadFloat(){std::uint32_t bits;Check(bool(std::cin>>bits),"short float-word input");float value;std::memcpy(&value,&bits,4);return value;}
std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
void Initialize(Volume& object)
{
    const std::array<Volume::Vector3,4> points{{{-2,-2,10},{2,-2,10},{2,2,10},{-2,2,10}}};
    spVertexBuffer vb;spIndexBuffer ib;std::vector<std::byte> bytes(48);std::memcpy(bytes.data(),points.data(),48);
    Check(vb.InitializeFromDataForAnalysis(0,4,0,bytes)&&ib.InitializeForAnalysis(2,spIndexBuffer::eIndexBufferType::Type2),"CPU quad buffers");
    const std::uint16_t indices[]{0,1,2,0,2,3};for(unsigned i=0;i<6;++i)Check(ib.SetIndexForAnalysis(i,indices[i]),"quad index");
    Check(object.InitializeForAnalysis(ib,vb,sparkplug::host::LegacySortPolicy::GeometryDispatch()),"fresh actual source geometry");
}
void CaptureBatch()
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=64,"bounded occluder case count");Volume volume;Initialize(volume);Manager manager;
    std::cout<<'[';
    for(std::size_t c=0;c<count;++c)
    {
        std::uint32_t frame,mark;unsigned unclipped,enabled;std::size_t planeCount;Manager::PlaneSetForAnalysis planes;
        Check(bool(std::cin>>frame>>mark>>unclipped>>enabled>>planeCount>>planes.activeCount)&&unclipped<=1&&enabled<=1&&planeCount<=64,"bounded occluder record");
        Volume::Vector3 position,scale;Volume::Matrix3 orientation;
        for(auto& value:position)value=ReadFloat();for(auto& value:scale)value=ReadFloat();for(auto& value:orientation)value=ReadFloat();
        planes.planes.resize(planeCount);for(auto& plane:planes.planes){for(auto& value:plane.equation)value=ReadFloat();unsigned flag;Check(bool(std::cin>>flag)&&flag<=255,"plane enabled input");plane.enabled=flag!=0;}
        volume.SetPositionForAnalysis(position);volume.SetScaleForAnalysis(scale);volume.SetOrientationForAnalysis(orientation);volume.MarkLocalTransformDirtyForAnalysis();Check(volume.UpdateWorldForAnalysis(),"actual source world update");volume.SetEnabledForAnalysis(enabled!=0,false);volume.SetVisibilityMarkForAnalysis(mark);
        std::uint32_t ignored=0;manager.BeginFrameForAnalysis(ignored);manager.SetFrameStampForAnalysis(frame);
        const auto first=manager.TrySubmitOccluderForAnalysis(volume,planes,unclipped!=0);
        const auto firstMark=volume.GetVisibilityMarkForAnalysis();
        const auto firstCount=manager.GetVisibleOccludersForAnalysis().size();
        const auto second=manager.TrySubmitOccluderForAnalysis(volume,planes,unclipped!=0);
        if(c)std::cout<<',';std::cout<<'['<<first.completed<<','<<first.submitted<<','<<firstMark<<','<<firstCount<<','<<second.completed<<','<<second.submitted<<','<<volume.GetVisibilityMarkForAnalysis()<<','<<manager.GetVisibleOccludersForAnalysis().size();
        for(float value:volume.GetWorldSphereForAnalysis())std::cout<<','<<Bits(value);std::cout<<']';
    }
    std::cout<<"]\n";
}
void Directed()
{
    Volume volume;Initialize(volume);volume.MarkLocalTransformDirtyForAnalysis();Check(volume.UpdateWorldForAnalysis(),"quad world update");volume.SetEnabledForAnalysis(false,false);
    Manager manager;manager.SetFrameStampForAnalysis(7);Manager::PlaneSetForAnalysis planes{{{{0,0,1,9},true}},1};
    auto result=manager.TrySubmitOccluderForAnalysis(volume,planes);Check(result.completed&&result.submitted&&volume.GetVisibilityMarkForAnalysis()==7,"intersecting sphere accepted by actual world vertices, disabled ignored");
    result=manager.TrySubmitOccluderForAnalysis(volume,planes);Check(result.completed&&!result.submitted&&manager.GetVisibleOccludersForAnalysis().size()==1,"stamp deduplicates repeated native-list entry");
    std::uint32_t stamp=0;manager.BeginFrameForAnalysis(stamp);Check(stamp==8&&manager.GetVisibleOccludersForAnalysis().empty()&&volume.GetVisibilityMarkForAnalysis()==7,"new frame clears borrowed output without changing old volume mark");
    planes.planes[0].equation[3]=11;result=manager.TrySubmitOccluderForAnalysis(volume,planes);Check(result.completed&&!result.submitted&&volume.GetVisibilityMarkForAnalysis()==7,"rejected point set is not stamped");
    result=manager.TrySubmitOccluderForAnalysis(volume,planes,true);Check(result.completed&&result.submitted&&volume.GetVisibilityMarkForAnalysis()==8,"unclipped branch stamps/appends without plane or Enabled tests");
    manager.BeginFrameForAnalysis(stamp);planes.activeCount=0;result=manager.TrySubmitOccluderForAnalysis(volume,planes);Check(result.completed&&result.submitted,"sphere Classify ignores count but point fallback observes cached zero");

    auto root=std::make_unique<spPartitionNode>();auto zone=std::make_shared<spZone>();root->SetZoneForAnalysis(zone);Check(zone->AppendRootForAnalysis(root.get()),"borrowed graph root");
    Manager::TraversalNodeForAnalysis node;node.spatial=root.get();node.occluders={&volume,&volume};Manager::SupportForAnalysis support;support.worldSphere={0,0,10,1};node.statics={&support};
    Manager::TraversalSceneForAnalysis scene;scene.partitionRoot=root.get();scene.nodes={&node};Manager::TraversalCameraForAnalysis camera;camera.farPlane.equation={0,0,-1,-1000};for(auto& side:camera.sides)side.equation={0,0,0,-1000};
    Check(manager.ProcessSceneForAnalysis(scene,&camera)==Manager::TraversalStatusForAnalysis::Complete&&manager.GetVisibleOccludersForAnalysis().size()==1&&manager.GetVisibleForAnalysis().size()==1,"actual graph traverses represented occluders after supports");
    const auto frame=manager.GetFrameStampForAnalysis();node.occluders.push_back(nullptr);Check(manager.ProcessSceneForAnalysis(scene,&camera)==Manager::TraversalStatusForAnalysis::InvalidInput&&manager.GetFrameStampForAnalysis()==frame,"host null-volume preflight precedes frame mutation");
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--occluder-batch"){CaptureBatch();return 0;}
    Directed();std::cout<<"PASS "<<checks<<"/"<<checks<<": represented occluder traversal\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
