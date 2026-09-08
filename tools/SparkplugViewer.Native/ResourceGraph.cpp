#include "ResourceGraph.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
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
    Register<spModel,spModelSerializer>(manager);
    Register<spSkin,spSkinSerializer>(manager);
    Register<spMeshData,spDXMeshDataSerializer>(manager);
    Register<spMaterialData,spMaterialDataSerializer>(manager);
    Register<spTextureData,spDXTextureDataSerializer>(manager);
    Register<spLightData,spLightDataSerializer>(manager);
    Register<spFog,spFogSerializer>(manager);
    Register<spCollisionInfo,spCollisionInfoSerializer>(manager);
    Register<spOBBBV,spOBBBVSerializer>(manager);
    Register<spMeshBV,spMeshBVSerializer>(manager);
    (void)winx::reconstruction::wxFaceData::StaticRTTI();
    Register<spUVController,spUVControllerSerializer>(manager);
    Register<spAnimTexController,spAnimTexControllerSerializer>(manager);
    Register<spMaterialColorController,spMatColorControllerSerializer>(manager);
    Register<spParticleSystem,spParticleSystemSerializer>(manager);
    spSerializerReadContextForAnalysis context(manager,resources);
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
