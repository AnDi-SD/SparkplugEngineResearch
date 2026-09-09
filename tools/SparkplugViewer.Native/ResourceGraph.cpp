#include "ResourceGraph.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spLensFlare.h"
#include "Code/Sparkplug/spLensFlareSerializer.h"
#include "Code/Sparkplug/spSkyBox.h"
#include "Code/Sparkplug/spNavigationGraph.h"
#include "Code/Sparkplug/spNavigationPortal.h"
#include "Code/Sparkplug/spMeshNavigationSet.h"
#include "Code/Sparkplug/spNavigationGraphSerializer.h"
#include "Code/Sparkplug/spNavigationPortalSerializer.h"
#include "Code/Sparkplug/spMeshNavigationSetSerializer.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spStaticRenderObjectSerializer.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spModelSerializer.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spMeshData.h"
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spTextureData.h"
#include "Code/Sparkplug/spDXTextureDataSerializer.h"
#include "Code/Sparkplug/spLightData.h"
#include "Code/Sparkplug/spLightDataSerializer.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spCollisionInfoSerializer.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/Sparkplug/spOBBBVSerializer.h"
#include "Code/Sparkplug/spSphereBV.h"
#include "Code/Sparkplug/spSphereBVSerializer.h"
#include "Code/Sparkplug/spBoxBV.h"
#include "Code/Sparkplug/spBoxBVSerializer.h"
#include "Code/Sparkplug/spMeshBV.h"
#include "Code/Sparkplug/spMeshBVSerializer.h"
#include "Code/wxFaceData.h"
#include "Code/Sparkplug/spUVController.h"
#include "Code/Sparkplug/spUVControllerSerializer.h"
#include "Code/Sparkplug/spAnimTexController.h"
#include "Code/Sparkplug/spAnimTexControllerSerializer.h"
#include "Code/Sparkplug/spMaterialColorController.h"
#include "Code/Sparkplug/spMatColorControllerSerializer.h"
#include "Code/Sparkplug/spParticleSystem.h"
#include "Code/Sparkplug/spParticleSystemSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spPartitionNodeSerializer.h"
#include "Code/Sparkplug/spBSPNode.h"
#include "Code/Sparkplug/spBSPNodeSerializer.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include "Code/Sparkplug/spOctreeNodeSerializer.h"
#include "Code/Sparkplug/spPartitionSystem.h"
#include "Code/Sparkplug/spPartitionSystemSerializer.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spZoneSerializer.h"
#include "Code/Sparkplug/spZonePortal.h"
#include "Code/Sparkplug/spZonePortalSerializer.h"
#include "Code/Sparkplug/spZonePortalNode.h"
#include "Code/Sparkplug/spZonePortalNodeSerializer.h"
#include "Code/Sparkplug/spPartitionRenderable.h"
#include "Code/Sparkplug/spPartitionRenderableSerializer.h"
#include <cstring>
#include <stdexcept>

namespace spvhost {
using namespace sparkplug::reconstruction;
namespace {
template<class T,class S> void Register(spSerializerManager& manager) {
    (void)T::StaticRTTI(); // keep the actual target's TU in the static-library link
    if(!manager.RegisterForAnalysis(T::ClassID,std::make_shared<S>(),255,1))
        throw std::runtime_error("Cannot register reconstructed resource reader");
}
}
std::shared_ptr<spPCRenderer> CpuRenderer() {
    // One declaration/cache owner, as required by the engine singleton. It has
    // no device and never enters legacy startup. Calls are serialized by C ABI.
    static auto renderer=std::make_shared<spPCRenderer>();return renderer;
}
ResourceGraph::ResourceGraph(const std::uint8_t* bytes,std::uint32_t count) {
    if(!bytes||count<36||count>64u*1024u*1024u)throw std::runtime_error("SMO must fit 64 MiB");
    renderer=CpuRenderer();
    // Cache/serializer lifetime is confined to this load. Separate documents
    // therefore cannot accidentally reuse a different file's same-name texture.
    spSerializerManager manager;spResourceManager resources;
    Register<spNode,spNodeSerializer>(manager);
    Register<spRenderNode,spRenderNodeSerializer>(manager);
    Register<spSkyBox,spRenderNodeSerializer>(manager); // exact original6D4B00 mapping
    Register<spLensFlare,spLensFlareSerializer>(manager);
    Register<spNavigationGraph,spNavigationGraphSerializer>(manager);
    Register<spNavigationPortal,spNavigationPortalSerializer>(manager);
    Register<spMeshNavigationSet,spMeshNavigationSetSerializer>(manager);
    Register<spStaticRenderObject,spStaticRenderObjectSerializer>(manager);
    Register<spModel,spModelSerializer>(manager);
    Register<spSkin,spSkinSerializer>(manager);
    Register<spMeshData,spDXMeshDataSerializer>(manager);
    Register<spMaterialData,spMaterialDataSerializer>(manager);
    Register<spTextureData,spDXTextureDataSerializer>(manager);
    Register<spLightData,spLightDataSerializer>(manager);
    Register<spFog,spFogSerializer>(manager);
    Register<spCollisionInfo,spCollisionInfoSerializer>(manager);
    Register<spOBBBV,spOBBBVSerializer>(manager);
    Register<spSphereBV,spSphereBVSerializer>(manager);
    Register<spBoxBV,spBoxBVSerializer>(manager);
    Register<spMeshBV,spMeshBVSerializer>(manager);
    (void)winx::reconstruction::wxFaceData::StaticRTTI();
    Register<spUVController,spUVControllerSerializer>(manager);
    Register<spAnimTexController,spAnimTexControllerSerializer>(manager);
    Register<spMaterialColorController,spMatColorControllerSerializer>(manager);
    Register<spParticleSystem,spParticleSystemSerializer>(manager);
    Register<spPartitionNode,spPartitionNodeSerializer>(manager);
    Register<spBSPNode,spBSPNodeSerializer>(manager);
    Register<spOctreeNode,spOctreeNodeSerializer>(manager);
    Register<spPartitionSystem,spPartitionSystemSerializer>(manager);
    Register<spZone,spZoneSerializer>(manager);
    Register<spZonePortal,spZonePortalSerializer>(manager);
    Register<spZonePortalNode,spZonePortalNodeSerializer>(manager);
    Register<spPartitionRenderable,spPartitionRenderableSerializer>(manager);
    spSerializerReadContextForAnalysis context(manager,resources);
    context.directOwnedClassIDsForAnalysis={spPartitionNode::ClassID,spPartitionRenderable::ClassID};
    // Host limit: pristine Alfea02 has4266 resources and reached4096 at a
    // measured47 MiB process peak. Keep a finite8192 ceiling and64 MiB input.
    context.maximumCreatedObjectsForAnalysis=8192;
    context.pcRenderer=renderer.get();context.captureFileObjectIDsForAnalysis=true;
    spMemoryStream input;
    if(!input.ResizeAndSetSize(count))throw std::runtime_error("Cannot allocate bounded SMO stream");
    std::memcpy(input.GetBuffer(),bytes,count);
    std::string error;
    auto* root=manager.LoadResourcesForAnalysis(input,context,&error);
    if(!root||context.failed)throw std::runtime_error(error.empty()?"Resource graph load failed":error);
    // Original generic loader can visit physical objects in a different order;
    // it need not leave the stream at EOF. Each serializer enforces its extent.
    entries=std::move(context.fileObjectsForAnalysis);
    owners=context.createdObjects;
    directOwners=std::move(context.pendingDirectObjectsForAnalysis);
    owners.insert(owners.end(),context.externalOwners.begin(),context.externalOwners.end());
    for(const auto& entry:entries) {
        if(!entry.object||!byID.emplace(entry.id,entry.object).second)
            throw std::runtime_error("Incomplete or duplicate resource identity");
        ids.emplace(entry.object,entry.id);
        if(dynamic_cast<spNode*>(entry.object))nodeIDs.push_back(entry.id);
    }
    rootID=ID(root);
    if(!rootID)throw std::runtime_error("Resource root lacks a file identity");
}
ResourceGraph::Object* ResourceGraph::Find(std::uint32_t id) const {
    auto item=byID.find(id);if(item==byID.end())throw std::runtime_error("Unknown file object ID");return item->second;
}
std::uint32_t ResourceGraph::ID(const Object* object) const {
    if(!object)return 0;auto item=ids.find(object);
    if(item==ids.end())throw std::runtime_error("Object is outside the owned resource graph");return item->second;
}
std::shared_ptr<spNode> ResourceGraph::Node(std::uint32_t id) const {
    auto* object=Find(id);
    for(const auto& owner:owners)if(owner.get()==object) {
        auto result=std::dynamic_pointer_cast<spNode>(owner);
        if(result)return result;break;
    }
    throw std::runtime_error("Expected an owned spNode");
}
}
