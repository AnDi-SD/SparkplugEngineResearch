// Own before-render observer. Each scope reads current native inputs afresh;
// no original producer is called and no last-visible packet is replayed.
#pragma once
#pragma push_macro("max")
#pragma push_macro("min")
#undef max
#undef min
#include "../../Sparkplug/Analysis/PC/spRenderNodeMath.h"
#include "../../Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h"
#pragma pop_macro("min")
#pragma pop_macro("max")
namespace independent_scene_source {
namespace abi=sparkplug::evidence::pc;
static bool enabled;
static bool submitEnabled,keepForComparison;
static void InitializeDirect();
static void DirectEndFrame();
static void InitializeSelected();
static void SelectedEndFrame();
static bool TrySelectedDraw(IDirect3DDevice9*,const native_mesh_source::DrawRange&,const ScopedOpaqueAlphaTest*);
static FILE* output;
static unsigned scans,candidates,selectedCandidates,dirtyCandidates,compared,matched,worldDifferences,materialDifferences;
static unsigned failureSamples;
static unsigned resourceCandidates,resourceCompared,resourceMatched,resourceDifferences,resourceRejected,uploadDifferences;
static unsigned drawCandidates,drawCompared,drawMatched,drawDifferences,drawRejected;
static unsigned textureCandidates,textureCompared,textureMatched,textureDifferences,textureRejected;
enum Reason:unsigned { Phase,Registry,Support,Hierarchy,World,Model,Callbacks,Material,Pass,Controllers,Override,Texture,Mapping,Capacity,Stale,Missing,Count };
static unsigned rejected[Count]{};
static bool Reject(Reason reason){++rejected[reason];return false;}
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
#if defined(_M_IX86)
struct DefaultStage {
  uint32_t material=0,pass=0,layer=0,holder=0,raw=0,selector=0,operation=0;
};
struct Input {
  native_owner_source::Packet owner{};
  native_update_source::Witness witness{};
  native_material_source::Packet material{};
  D3DMATRIX inverse{};
  uint32_t materialStates[11]{};
  std::array<uint32_t,256> mappedStates{};
  std::array<uint8_t,256> mappedPresent{};
  uint32_t modelOrdinal=0,textureCOM=0;
  uint8_t effectiveOverride=0;
  bool computedWorld=false,originallySelected=false;
  bool resourcesReady=false;
  // Value-only resource stamp. Borrowed Bytes/layout pointers are cleared
  // before storing; later operations must obtain their own fresh borrow.
  native_mesh_source::Geometry resources{};
  uint64_t cpuGeneration=0;
  SurfaceInstanceState instance{};
  surface_material::ObservedStage bootstrapStage{};
  DWORD textureSrgb=0,separateAlpha=0,stencil=0;
  bool drawReady=false;
  DefaultStage defaultStage{};
  native_transport_source::TextureWitness textureWitness{};
};
using Key=std::pair<uint32_t,uint32_t>; // support + Model, within one scene operation only
static std::map<Key,Input> inputs;
static uint64_t inputScope;
static uint32_t inputSelectionManager;
static void BeginDirectScope();
static void SubmitCurrent(const std::unique_lock<std::recursive_mutex>& borrow,scene_geometry::Scope& scope);
static bool SkipDirectDraw(IDirect3DDevice9*,const native_mesh_source::DrawRange&);
static DWORD ownerThread;
constexpr unsigned inputLimit=16384;
#if defined(WINX_REMIX_TEST)
static uintptr_t queueModeAddress=0x75f8e8;
#else
static constexpr uintptr_t queueModeAddress=0x75f8e8;
#endif
template<class T> static bool Read(uintptr_t address,T& value){return native_owner_source::Read(address,value);}
static bool EmptyCallbacks(const abi::spRenderableCallbackVectorLayout& value) {
  return value.begin<=value.end&&value.end<=value.capacityEnd&&value.begin==value.end&&
    (!value.begin||value.begin>=0x10000);
}
struct Lighting {
  std::array<float,4> diffuse{},ambient{},specular{},emissive{};
  float specularPower=0;
  uint32_t packedColorC194=0,globalBlackARGB=0,diffuseSource=~0u,ambientSource=~0u;
};
struct Overrides {
  const std::array<uint32_t,11>* const* sources=nullptr;
  size_t count=0;
  std::array<uint32_t,10> selectors{};
};
static int32_t MapRenderState(void* context,uint32_t index,uint32_t value) noexcept {
  auto& input=*static_cast<Input*>(context);
  if(index>=input.mappedStates.size())return -1;
  input.mappedStates[index]=value;input.mappedPresent[index]=1;return 0;
}
static bool Finite(const D3DMATRIX& matrix) {
  for(const auto& row:matrix.m)for(float value:row)if(!std::isfinite(value))return false;
  return true;
}
static bool ChildMembership(uint32_t parent,uint32_t child) {
  const auto head=scene_geometry::Word(parent+0x18);if(!head)return false;
  auto entry=scene_geometry::Word(head);
  for(unsigned i=0;i<scene_geometry::limit&&entry&&entry!=head;++i) {
    uint32_t link[3]{};if(!Read(entry,link))return false;
    if(link[2]==child)return true;
    entry=link[0];
  }
  return false;
}
static bool KnownWorldSlot(uint32_t table) {
  const auto slot=scene_geometry::Word(table+0x30);
  return slot==0x4250f0||native_update_source::IsPlainWorldSlot(slot);
}
static bool HierarchyCurrent(uint32_t object,const native_update_source::Witness& witness) {
  uint32_t visited[128]{},count=0,current=object;
  while(current&&count<128) {
    for(unsigned i=0;i<count;++i)if(visited[i]==current)return false;
    visited[count++]=current;
    abi::spNodeLayout node{};
    if(!Read(current,node)||!KnownWorldSlot(node.base.base.vtableAddress)||(node.flags&7))return false;
    if(current==witness.systemRoot)return true;
    if(node.sceneLink!=witness.scene||!node.parent||!ChildMembership(node.parent,current))return false;
    current=node.parent;
  }
  return false;
}
static bool WorldInput(Input& input,const abi::spRenderSupportObservedLayout& support) {
  auto& owner=input.owner;
  if(!native_owner_source::SupportIdentity(owner,support))return Reject(Support);
  if(support.vtable==abi::spRenderNodeSupportVTable) {
    abi::spRenderNodeLayout node{};
    if(!Read(owner.object,node)||(node.prefix.base.flags&0x300007)||
       !(node.prefix.base.flags&0x200))return Reject(World);
    // CP12: these are reciprocal partition membership/lifecycle listeners,
    // not Model Pre/Post callbacks. Ordinary registered nodes have entries.
    // Their native update/Enabled/transfer/destruction notifications stay intact.
    if(node.callbackBegin>node.callbackEnd||node.callbackEnd>node.callbackCapacity||
       (node.callbackEnd-node.callbackBegin)%4||node.callbackEnd-node.callbackBegin>4096*4)return Reject(Registry);
    if(!HierarchyCurrent(owner.object,input.witness))return Reject(Hierarchy);
    abi::node_math::Vector3 position{},scale{},inverseScale{};abi::node_math::Matrix3 orientation{};
    memcpy(position.data(),node.prefix.base.cachedWorldPosition,sizeof(position));
    memcpy(scale.data(),node.prefix.base.cachedWorldScale,sizeof(scale));
    memcpy(orientation.data(),node.prefix.base.cachedWorldOrientation,sizeof(orientation));
    memcpy(inverseScale.data(),node.inverseWorldScale,sizeof(inverseScale));
    if(!abi::render_node_math::Finite(position)||!abi::render_node_math::Finite(scale)||
       !abi::render_node_math::Finite(orientation)||!abi::render_node_math::Finite(inverseScale)||
       !scale[0]||!scale[1]||!scale[2])return Reject(World);
    input.computedWorld=(node.matrixDirty&1)!=0;
    if(input.computedWorld) {
      const auto world=abi::node_math::Affine(position,orientation,scale);
      const auto inverse=abi::render_node_math::InversePRS(position,orientation,inverseScale);
      memcpy(&owner.world,world.data(),sizeof(owner.world));memcpy(&input.inverse,inverse.data(),sizeof(input.inverse));
    } else {
      memcpy(&owner.world,node.worldMatrix,sizeof(owner.world));memcpy(&input.inverse,node.inverseWorldMatrix,sizeof(input.inverse));
    }
  } else if(!Read(support.worldMatrixPointer,owner.world)||!Read(support.inverseMatrixPointer,input.inverse))return Reject(World);
  return Finite(owner.world)&&Finite(input.inverse)?true:Reject(World);
}
static bool MaterialInput(Input& input,const abi::spModelLayout& model,uint32_t device,uint8_t outerOverride) {
  abi::spDXMaterialObservedLayout material{};
  if(!model.base.material||!Read(model.base.material,material)||
     material.base.base.vtableAddress!=abi::spDXMaterialPrimaryVTable||
     material.base.materialVTable!=abi::spDXMaterialInterfaceVTable||material.base.renderStates[8]!=2)return Reject(Material);
  if(material.base.materialColorController)return Reject(Controllers);
  abi::spMaterialPassLayerObservedLayout pass{};
  if(material.base.passCount!=1||!Read(material.base.passes[0],pass)||
     pass.base.vtableAddress!=abi::spMaterialPassLayerVTable||pass.layerCount!=1||pass.finalBlendOperation)return Reject(Pass);
  abi::spStdLayerObservedLayout layer{};abi::spMaterialTextureObservedLayout texture{};
  if(!Read(pass.layers[0],layer)||layer.base.base.vtableAddress!=abi::spStdLayerVTable||
     !Read(layer.base.materialTexture,texture)||texture.base.vtableAddress!=abi::spMaterialTextureVTable)return Reject(Texture);
  if(texture.uvController||texture.animationController||texture.hasStaticUV)return Reject(Controllers);
  abi::spDXTextureObservedLayout nativeTexture{};
  if(!Read(texture.fallbackTexture,nativeTexture)||nativeTexture.base.base.base.base.vtableAddress!=abi::spDXTexturePrimaryVTable||
     nativeTexture.device!=device||!nativeTexture.texture)return Reject(Texture);
  input.effectiveOverride=material.base.renderOverride?0:outerOverride;
  if(input.effectiveOverride) {
    uint32_t selectors[11]{};if(!Read(input.owner.renderer+0xc71c,selectors))return Reject(Override);
    for(unsigned i=1;i<11;++i)if(selectors[i])return Reject(Override);
  }
  native_material_source::Cache cache{};native_material_source::Mapped mapped;
  for(unsigned i=1;i<9;++i)
    if(!sparkplug::reconstruction::ApplyPCTextureStateForAnalysis(cache,0,i,texture.textureStates[i],false,
       native_material_source::Map,&mapped))return Reject(Mapping);
  if(!mapped.valid||mapped.stage[11]||mapped.stage[24])return Reject(Mapping);
  auto& out=input.material;out.renderer=input.owner.renderer;out.material=model.base.material;
  out.pass=material.base.passes[0];out.layer=pass.layers[0];out.textureOwner=layer.base.materialTexture;
  out.texture=texture.fallbackTexture;memcpy(out.raw,texture.textureStates,sizeof(out.raw));
  out.contract.coordinates=mapped.stage[11];out.contract.transformFlags=mapped.stage[24];
  // Arguments are an explicit bootstrap condition, compared again at actual
  // draw. This packet does not claim they originate in the raw texture words.
  if(!surface_material::Decode(mapped.stage[1],D3DTA_TEXTURE,D3DTA_CURRENT,out.contract.rgb)||
     !surface_material::DecodeAlpha(mapped.stage[4],D3DTA_TEXTURE,D3DTA_CURRENT,out.contract.alpha))return Reject(Mapping);
  out.sampler={mapped.sampler[1],mapped.sampler[2],mapped.sampler[5],mapped.sampler[6],mapped.sampler[7]};
  memcpy(input.materialStates,material.base.renderStates,sizeof(input.materialStates));
  input.materialStates[7]=pass.finalBlendOperation; // CP34 local copy of native pass write
  std::array<uint32_t,11> raw{},source{};raw.fill(~0u);memcpy(source.data(),input.materialStates,sizeof(input.materialStates));
  Lighting lighting{};
  // Mode2's output states do not consume material colors, power, packed color
  // or global black. Local zero storage is explicitly unconsumed in this cohort.
  // Force initial cache misses to describe the full expected emitted state set.
  if(!sparkplug::reconstruction::ApplyPCMaterialStateSetForAnalysis(raw,source,static_cast<const Overrides*>(nullptr),
     lighting,MapRenderState,&input))return Reject(Mapping);
  input.textureCOM=nativeTexture.texture;return true;
}
static bool ResourceInput(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,Input& input) {
  abi::spDXMeshObservedLayout mesh{};native_mesh_source::ResourcePair pair{};uint32_t declaration[7]{};
  if(!Read(input.owner.mesh,mesh)||!native_mesh_source::ReadResourceHeaders(mesh,pair)||!Read(mesh.vertexDeclaration,declaration))return false;
  native_mesh_source::TransportWitness transport{};native_mesh_source::Geometry geometry{};
  if(!native_transport_source::Witness(borrow,device,reinterpret_cast<void*>(pair.vb.direct3DVertexBuffer),
       reinterpret_cast<void*>(pair.ib.direct3DIndexBuffer),reinterpret_cast<void*>(declaration[6]),transport)||
     !native_mesh_source::ResolveCurrentResources(borrow,device,input.owner.mesh,input.owner.renderer,input.owner.world,transport,geometry))return false;
  const auto vertices=surfaceBuffers.find(geometry.vertexBuffer),indices=surfaceBuffers.find(geometry.indexBuffer);
  if(vertices==surfaceBuffers.end()||indices==surfaceBuffers.end()||!vertices->second.complete||!indices->second.complete)return false;
  if(!geometry.vertices->verified||!geometry.indices->verified) {
    if(!native_mesh_source::EqualsUpload(*geometry.vertices,vertices->second.bytes)||
       !native_mesh_source::EqualsUpload(*geometry.indices,indices->second.bytes)){++uploadDifferences;return false;}
    // Existing own flag now records the same complete upload comparison before
    // a draw. Native resource/cache fields are never modified by this reader.
    geometry.vertices->verified=true;geometry.indices->verified=true;
  }
  if(!native_transport_source::Current(borrow,transport))return false;
  input.resources=geometry;input.cpuGeneration=geometry.vertices->generation;
  input.resources.vertices=nullptr;input.resources.indices=nullptr;input.resources.layout=nullptr;
  input.resourcesReady=true;return true;
}
// The remaining bootstrap inputs are read once at the current scene boundary.
// Native material mapping supplies its own states; previous bound material
// operations are never used as their substitute. No device state is changed.
struct DrawBootstrap {
  SurfaceInstanceState instance{};
  surface_material::ObservedStage stage{};
  DWORD separateAlpha=0,stencil=0,srgb=0,factor=0;
  DefaultStage defaultStage{};
  bool valid=false;
};
static int32_t MapDefaultStage(void* context,bool sampler,uint32_t stage,uint32_t index,uint32_t value) noexcept {
  if(sampler||stage!=1||index!=1)return -1;
  static_cast<DefaultStage*>(context)->operation=value;return 0;
}
static bool ReadDefaultStage(uint32_t renderer,DefaultStage& out) {
  out={};abi::spDXMaterialObservedLayout material{};abi::spMaterialPassLayerObservedLayout pass{};
  abi::spStdLayerObservedLayout layer{};abi::spMaterialTextureObservedLayout holder{};
  if(!Read(renderer+0xc9c0,out.material)||!Read(out.material,material)||
     material.base.base.vtableAddress!=abi::spDXMaterialPrimaryVTable||material.base.materialVTable!=abi::spDXMaterialInterfaceVTable)return false;
  out.pass=material.base.passes[0];
  if(!Read(out.pass,pass)||pass.base.vtableAddress!=abi::spMaterialPassLayerVTable)return false;
  // CP36 reads the fixed default slot1 even when layerCount says1. No getter,
  // controller or fallbackTexture call participates in this unused-stage copy.
  out.layer=pass.layers[1];
  if(!Read(out.layer,layer)||layer.base.base.vtableAddress!=abi::spStdLayerVTable)return false;
  out.holder=layer.base.materialTexture;
  if(!Read(out.holder,holder)||holder.base.vtableAddress!=abi::spMaterialTextureVTable||
     !Read(renderer+0xc770,out.selector)||out.selector)return false;
  out.raw=holder.textureStates[1];native_material_source::Cache cache{};
  return sparkplug::reconstruction::ApplyPCTextureStateForAnalysis(cache,1,1,out.raw,false,MapDefaultStage,&out)&&out.operation==D3DTOP_DISABLE;
}
static bool DefaultStageCurrent(uint32_t renderer,const DefaultStage& input) {
  DefaultStage current{};return ReadDefaultStage(renderer,current)&&!memcmp(&current,&input,sizeof(input));
}
static bool ReadDrawBootstrap(IDirect3DDevice9* device,uint32_t renderer,DrawBootstrap& input) {
  input.valid=false;
  if(!device||!ReadDefaultStage(renderer,input.defaultStage))return false;
  const auto state=[&](D3DRENDERSTATETYPE type,DWORD& value){
    return SUCCEEDED(device->GetRenderState(type,&value));
  };
  auto& out=input.instance;out.channels=true;
  if(!state(D3DRS_ALPHATESTENABLE,out.test)||
     !state(D3DRS_COLORWRITEENABLE,out.writeMask)||!state(D3DRS_ALPHABLENDENABLE,out.blendEnabled)||
     !state(D3DRS_BLENDOP,out.operation)||
     !state(D3DRS_SEPARATEALPHABLENDENABLE,input.separateAlpha)||input.separateAlpha||
     !state(D3DRS_STENCILENABLE,input.stencil)||input.stencil||
     FAILED(device->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&input.srgb))||input.srgb)return false;
  auto& stage=input.stage;
  const auto get=[&](D3DTEXTURESTAGESTATETYPE type,DWORD& value){return SUCCEEDED(device->GetTextureStageState(0,type,&value));};
  if(!get(D3DTSS_COLORARG1,stage.r1)||!get(D3DTSS_COLORARG2,stage.r2)||
     !get(D3DTSS_ALPHAARG1,stage.a1)||!get(D3DTSS_ALPHAARG2,stage.a2)||
     !get(D3DTSS_RESULTARG,stage.result)||stage.result!=D3DTA_CURRENT||
     FAILED(device->GetRenderState(D3DRS_TEXTUREFACTOR,&input.factor)))return false;
  stage.next=input.defaultStage.operation;
  // The native raw-state cache may suppress a repeated setter. Preserve the
  // actual disabled state as well as the current intended default value.
  DWORD currentNext=0;
  if(FAILED(device->GetTextureStageState(1,D3DTSS_COLOROP,&currentNext))||currentNext!=stage.next)return false;
  // Stage zero CURRENT and DIFFUSE are equivalent here. Keep exact bootstrap
  // words for comparison, and reject modifiers or a different source equation.
  if(stage.r1!=D3DTA_TEXTURE||(stage.r2!=D3DTA_CURRENT&&stage.r2!=D3DTA_DIFFUSE)||
     stage.a1!=D3DTA_TEXTURE||(stage.a2!=D3DTA_CURRENT&&stage.a2!=D3DTA_DIFFUSE))return false;
  input.valid=true;return true;
}
static bool DrawInput(const DrawBootstrap& bootstrap,Input& input) {
  input.drawReady=false;
  if(!bootstrap.valid)return false;
  input.instance=bootstrap.instance;input.bootstrapStage=bootstrap.stage;
  input.defaultStage=bootstrap.defaultStage;
  input.textureSrgb=bootstrap.srgb;input.separateAlpha=bootstrap.separateAlpha;input.stencil=bootstrap.stencil;
  input.material.contract.factor=bootstrap.factor;
  const auto state=[&](D3DRENDERSTATETYPE type,DWORD& value){
    const auto index=static_cast<unsigned>(type);
    if(index>=input.mappedPresent.size()||!input.mappedPresent[index])return false;
    value=input.mappedStates[index];return true;
  };
  auto& out=input.instance;
  if(!state(D3DRS_CULLMODE,out.cull)||!state(D3DRS_ALPHAFUNC,out.function)||!state(D3DRS_ALPHAREF,out.reference)||
     !state(D3DRS_SRCBLEND,out.source)||!state(D3DRS_DESTBLEND,out.destination))return false;
  remixapi_InstanceInfoBlendEXT blend{};remixapi_InstanceInfo instance{};
  if(!material_channels::Texture(input.material.contract,input.material.contract)||
     !DescribeSurfaceInstance(out,input.material.contract,input.owner.world,reinterpret_cast<remixapi_MeshHandle>(1),blend,instance))return false;
  input.drawReady=true;return true;
}
static void Observe(scene_geometry::Scope& scope,const scene_geometry::Registry& registry,uint32_t selection) {
  if(!enabled||GetCurrentThreadId()!=ownerThread)return;
  std::unique_lock<std::recursive_mutex> borrow(guard);
  BeginDirectScope();inputs.clear();inputScope=0;++scans;
  if(scope.parent||native_owner_source::activeModel||native_owner_source::activeSupport||
     !registry.valid||!registry.occurrencesComplete||registry.mutationSerial!=scene_geometry::MutationSerial()) {Reject(Registry);return;}
  native_update_source::Witness witness{};
  if(!native_update_source::GetWitness(scope.scene,frameId,registry.mutationSerial,witness)){Reject(Phase);return;}
  const auto renderer=scene_geometry::Word(native_owner_source::rendererPointerAddress);
  abi::spRendererDrawContextObservedLayout state{};uint8_t queueMode=1;
  uint32_t selectors[9]{};struct Shader {uint32_t flags[8],selected;} shader{};
  if(!renderer||scene_geometry::Word(renderer)!=abi::spPCRendererPrimaryVTable||
     !Read(renderer+abi::spRendererDrawContextOffset,state)||state.materialOverride||
     !Read(queueModeAddress,queueMode)||queueMode||!Read(renderer+0xc748,selectors)||
     !Read(renderer+0xe454,shader)||shader.selected>=8||shader.flags[shader.selected]) {Reject(Override);return;}
  for(unsigned i=1;i<9;++i)if(selectors[i]){Reject(Override);return;}
  uint8_t outerOverride=0;if(!Read(renderer+0xc1c4,outerOverride)){Reject(Override);return;}
  DrawBootstrap bootstrap{};ReadDrawBootstrap(reinterpret_cast<IDirect3DDevice9*>(state.device),renderer,bootstrap);
  std::vector<uint32_t> selected;
  if(!scene_geometry::Vector(selection,0x28,selected)){Reject(Registry);return;}
  std::set<uint32_t> selectedSet(selected.begin(),selected.end());
  std::map<uint32_t,unsigned> registrations;
  for(const auto& occurrence:registry.occurrences)++registrations[occurrence.support];
  try {
    for(uint32_t supportAddress:registry.supports) {
      abi::spRenderSupportObservedLayout support{};if(!Read(supportAddress,support)){Reject(Support);continue;}
      Input base{};base.witness=witness;base.originallySelected=selectedSet.count(supportAddress)!=0;
      auto& owner=base.owner;owner.scene=scope.scene;owner.system=registry.system;owner.root=registry.root;
      owner.camera=scope.camera;owner.support=supportAddress;owner.object=support.completeObject;
      owner.renderer=renderer;owner.frame=frameId;owner.sceneScope=scope.serial;owner.mutation=registry.mutationSerial;
      owner.registrations=registrations[supportAddress];owner.ownerPrimary=scene_geometry::Word(owner.object);
      owner.ownerKind=support.vtable==abi::spRenderNodeSupportVTable?2:support.vtable==0x6f4528?1:0;
      if(!owner.ownerPrimary||!WorldInput(base,support))continue;
      if(support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
         (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4){Reject(Support);continue;}
      std::map<uint32_t,std::pair<unsigned,unsigned>> models;
      bool readable=true;
      for(unsigned ordinal=0;ordinal<(support.renderableEnd-support.renderableBegin)/4;++ordinal) {
        uint32_t model=0;if(!Read(support.renderableBegin+ordinal*4,model)){readable=false;break;}
        if(model){auto& entry=models[model];if(!entry.first)entry.second=ordinal;++entry.first;}
      }
      if(!readable){Reject(Support);continue;}
      for(const auto& entry:models) {
        abi::spModelLayout model{};if(!Read(entry.first,model)||model.base.base.base.vtableAddress!=0x6eaa58||!model.baseMeshData){Reject(Model);continue;}
        if(model.base.callback2C||model.base.callback30||!EmptyCallbacks(model.base.callbacks34)||!EmptyCallbacks(model.base.callbacks44)){Reject(Callbacks);continue;}
        Input input=base;input.owner.model=entry.first;input.owner.mesh=model.baseMeshData;input.owner.modelMaterial=model.base.material;
        input.owner.modelOccurrences=entry.second.first;input.owner.firstModelOrdinal=entry.second.second;
        input.modelOrdinal=entry.second.second;
        if(!MaterialInput(input,model,state.device,outerOverride))continue;
        ResourceInput(borrow,reinterpret_cast<IDirect3DDevice9*>(state.device),input);
        DrawInput(bootstrap,input);
        native_transport_source::BorrowTexture(borrow,reinterpret_cast<IDirect3DDevice9*>(state.device),
          reinterpret_cast<IDirect3DTexture9*>(input.textureCOM),input.textureWitness);
        if(inputs.size()>=inputLimit){inputs.clear();Reject(Capacity);return;}
        input.owner.valid=true;inputs.emplace(Key{supportAddress,entry.first},input);
      }
    }
  } catch(const std::bad_alloc&) {inputs.clear();Reject(Capacity);return;}
  if(!native_update_source::QualifyWitness(witness,scope.scene,frameId,registry.mutationSerial)||
     registry.mutationSerial!=scene_geometry::MutationSerial()) {inputs.clear();Reject(Stale);return;}
  if(bootstrap.valid&&!DefaultStageCurrent(renderer,bootstrap.defaultStage)) {
    for(auto& item:inputs)item.second.drawReady=false;
  }
  inputScope=scope.serial;inputSelectionManager=selection;
  for(const auto& pair:inputs) {
    ++candidates;if(pair.second.originallySelected)++selectedCandidates;if(pair.second.computedWorld)++dirtyCandidates;
    if(pair.second.resourcesReady)++resourceCandidates;else ++resourceRejected;
    if(pair.second.drawReady)++drawCandidates;else ++drawRejected;
    if(pair.second.textureWitness.valid)++textureCandidates;else ++textureRejected;
  }
  if(CanLog()&&(frameId%300==0||triggered))fprintf(output,
    "{\"event\":\"scan\",\"frame\":%u,\"scene\":%u,\"scope\":%llu,\"update\":%llu,\"mutation\":%llu,\"packets\":%zu,\"source\":\"current_graph_before_prepare\"}\n",
    frameId,scope.scene,scope.serial,witness.sequence,witness.mutation,inputs.size());
  if(submitEnabled&&!keepForComparison)SubmitCurrent(borrow,scope);
}
static bool MatrixMatches(const D3DMATRIX& expected,const D3DMATRIX& actual,bool computed,float& maximum) {
  maximum=0;
  if(!computed)return memcmp(&expected,&actual,sizeof(actual))==0;
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
    const float a=expected.m[r][c],b=actual.m[r][c],error=std::fabs(a-b);
    if(!std::isfinite(a)||!std::isfinite(b))return false;
    maximum=(std::max)(maximum,error);
    // Analytical float helper vs original x87: explicit comparison tolerance,
    // never a claim of bit-identical production arithmetic.
    if(error>1e-5f+(std::max)(std::fabs(a),std::fabs(b))*1e-6f)return false;
  }
  return true;
}
static unsigned CompareDrawInput(IDirect3DDevice9* device,const Input& input) {
  if(!device)return ~0u;
  const auto& state=input.instance;unsigned differences=0;
  const DWORD expected[]={state.test,state.blendEnabled,state.operation,state.writeMask,input.separateAlpha,input.stencil};
  const D3DRENDERSTATETYPE types[]={D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_BLENDOP,D3DRS_COLORWRITEENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_STENCILENABLE};
  for(unsigned i=0;i<6;++i){DWORD actual=0;if(FAILED(device->GetRenderState(types[i],&actual))||actual!=expected[i])differences|=1u<<i;}
  const auto& stage=input.bootstrapStage;
  const DWORD args[]={stage.r1,stage.r2,stage.a1,stage.a2,stage.result};
  const D3DTEXTURESTAGESTATETYPE stages[]={D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_RESULTARG};
  for(unsigned i=0;i<5;++i){DWORD actual=0;if(FAILED(device->GetTextureStageState(0,stages[i],&actual))||actual!=args[i])differences|=1u<<(i+6);}
  DWORD next=0,factor=0,srgb=0;
  if(FAILED(device->GetTextureStageState(1,D3DTSS_COLOROP,&next))||next!=stage.next)differences|=1u<<11;
  if(FAILED(device->GetRenderState(D3DRS_TEXTUREFACTOR,&factor))||factor!=input.material.contract.factor)differences|=1u<<12;
  if(FAILED(device->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb))||srgb!=input.textureSrgb)differences|=1u<<13;
  return differences;
}
static bool Compare(const native_owner_source::Packet& owner,const native_material_source::Packet* material,IDirect3DDevice9* device,
                    const ScopedOpaqueAlphaTest* alphaNormalization,const native_mesh_source::Geometry* geometry) {
  if(!enabled||!owner.valid||GetCurrentThreadId()!=ownerThread)return false;
  if(inputScope!=owner.sceneScope){return Reject(Missing);}
  const auto found=inputs.find({owner.support,owner.model});if(found==inputs.end())return Reject(Missing);
  const auto& input=found->second;
  if(!native_update_source::QualifyWitness(input.witness,owner.scene,frameId,owner.mutation)||
     !native_owner_source::Current(owner)||owner.mesh!=input.owner.mesh||owner.ownerPrimary!=input.owner.ownerPrimary||
     owner.modelOccurrences!=input.owner.modelOccurrences||owner.firstModelOrdinal!=input.owner.firstModelOrdinal)return Reject(Stale);
  float error=0;
  D3DMATRIX inverse{};float inverseError=0;
  abi::spRenderSupportObservedLayout support{};
  const bool world=MatrixMatches(input.owner.world,owner.world,input.computedWorld,error)&&Read(owner.support,support)&&
    Read(support.inverseMatrixPointer,inverse)&&MatrixMatches(input.inverse,inverse,input.computedWorld,inverseError);
  bool materialMatches=material&&material->material==input.material.material&&material->pass==input.material.pass&&
    material->layer==input.material.layer&&material->textureOwner==input.material.textureOwner&&material->texture==input.material.texture&&
    !memcmp(material->raw,input.material.raw,sizeof(input.material.raw))&&
    !memcmp(&material->sampler,&input.material.sampler,sizeof(input.material.sampler))&&
    !memcmp(&material->contract.rgb,&input.material.contract.rgb,sizeof(surface_material::Channel))&&
    !memcmp(&material->contract.alpha,&input.material.contract.alpha,sizeof(surface_material::Channel));
  uint32_t materialReasons=materialMatches?0:1,rawDifferenceMask=0;
  uint32_t deviceDifferenceCount=0,firstDeviceDifference=0,firstDeviceExpected=0,firstDeviceActual=0;
  bool recoveredAlpha=false;
  abi::spRendererDrawContextObservedLayout state{};
  if(!Read(owner.renderer+abi::spRendererDrawContextOffset,state)){materialMatches=false;materialReasons|=2;}
  else for(unsigned i=1;i<11;++i)if(state.renderStates[i]!=input.materialStates[i]){materialMatches=false;materialReasons|=2;rawDifferenceMask|=1u<<i;}
  uint8_t effectiveOverride=0;abi::spDXTextureObservedLayout texture{};
  if(!Read(owner.renderer+0xc1c4,effectiveOverride)||effectiveOverride!=input.effectiveOverride){materialMatches=false;materialReasons|=4;}
  if(!Read(input.material.texture,texture)||texture.texture!=input.textureCOM){materialMatches=false;materialReasons|=8;}
  for(unsigned i=0;i<input.mappedPresent.size();++i)if(input.mappedPresent[i]) {
    DWORD actual=0;
    const auto type=static_cast<D3DRENDERSTATETYPE>(i);
    const bool read=device&&SUCCEEDED(device->GetRenderState(type,&actual));DWORD nativeInput=actual;
    if(read&&alphaNormalization&&alphaNormalization->RecoverInput(device,type,actual,nativeInput))recoveredAlpha=true;
    if(!read||nativeInput!=input.mappedStates[i]) {
      materialMatches=false;materialReasons|=16;
      if(!deviceDifferenceCount){firstDeviceDifference=i;firstDeviceExpected=input.mappedStates[i];firstDeviceActual=actual;}
      ++deviceDifferenceCount;
    }
  }
  bool resourcesMatch=false;
  if(input.resourcesReady&&geometry&&geometry->vertices&&geometry->indices) {
    std::unique_lock<std::recursive_mutex> borrow(guard);
    native_mesh_source::TransportWitness transport{};
    const auto& expected=input.resources;
    resourcesMatch=native_transport_source::Witness(borrow,device,expected.vertexBuffer,expected.indexBuffer,expected.declaration,transport)&&
      transport.vertexGeneration==expected.vertexGeneration&&transport.indexGeneration==expected.indexGeneration&&
      transport.declarationGeneration==expected.declarationGeneration&&geometry->vertices->generation==input.cpuGeneration&&
      geometry->indices->generation==input.cpuGeneration&&geometry->stride==expected.stride&&geometry->componentFlags==expected.componentFlags&&
      !memcmp(&geometry->range,&expected.range,sizeof(expected.range));
  }
  auto drawReasons=input.drawReady?CompareDrawInput(device,input):0;
  if(input.drawReady&&!DefaultStageCurrent(owner.renderer,input.defaultStage))drawReasons|=1u<<14;
  bool textureMatches=false;
  if(input.textureWitness.valid) {
    std::unique_lock<std::recursive_mutex> borrow(guard);
    textureMatches=texture.texture==input.textureCOM&&native_transport_source::CurrentTexture(borrow,input.textureWitness);
  }
  // Device/native reads can overlap a known foreign update or retirement.
  // Credit belongs only to a packet still current after every borrowed read.
  if(!native_update_source::QualifyWitness(input.witness,owner.scene,frameId,owner.mutation)||
     !native_owner_source::Current(owner))return Reject(Stale);
  ++compared;
  if(input.resourcesReady){++resourceCompared;if(resourcesMatch)++resourceMatched;else ++resourceDifferences;}
  if(input.drawReady){++drawCompared;if(!drawReasons)++drawMatched;else ++drawDifferences;}
  if(input.textureWitness.valid){++textureCompared;if(textureMatches)++textureMatched;else ++textureDifferences;}
  if(!world)++worldDifferences;if(!materialMatches)++materialDifferences;
  if(world&&materialMatches)++matched;
  const bool failureSample=(!world||!materialMatches||(input.resourcesReady&&!resourcesMatch)||drawReasons||
    (input.textureWitness.valid&&!textureMatches))&&failureSamples++<8;
  if(CanLog()&&((frameId%300==0||triggered)||failureSample))fprintf(output,
    "{\"event\":\"compare\",\"frame\":%u,\"scope\":%llu,\"update\":%llu,\"support\":%u,\"model\":%u,\"kind\":%u,\"computedWorld\":%s,\"originallySelected\":%s,\"world\":%s,\"material\":%s,\"maxWorldError\":%.9g,\"maxInverseError\":%.9g,\"materialReasons\":%u,\"rawDifferenceMask\":%u,\"overrideExpected\":%u,\"overrideActual\":%u,\"deviceDifferences\":%u,\"firstDeviceDifference\":[%u,%u,%u],\"alphaInputBeforeAdapter\":%s,\"resourcesReady\":%s,\"resourcesMatch\":%s,\"cpuGeneration\":%llu,\"drawReady\":%s,\"drawReasons\":%u,\"textureReady\":%s,\"textureMatch\":%s,\"textureGeneration\":%llu,\"textureContent\":%llu}\n",
    frameId,owner.sceneScope,input.witness.sequence,owner.support,owner.model,owner.ownerKind,
    input.computedWorld?"true":"false",input.originallySelected?"true":"false",world?"true":"false",materialMatches?"true":"false",error,inverseError,
    materialReasons,rawDifferenceMask,input.effectiveOverride,effectiveOverride,deviceDifferenceCount,firstDeviceDifference,firstDeviceExpected,firstDeviceActual,recoveredAlpha?"true":"false",
    input.resourcesReady?"true":"false",resourcesMatch?"true":"false",input.cpuGeneration,input.drawReady?"true":"false",drawReasons,input.textureWitness.valid?"true":"false",textureMatches?"true":"false",input.textureWitness.generation,input.textureWitness.contentGeneration);
  return world&&materialMatches;
}
#else
static bool Compare(const native_owner_source::Packet&,const native_material_source::Packet*,IDirect3DDevice9*,const ScopedOpaqueAlphaTest*,const native_mesh_source::Geometry*){return false;}
static bool SkipDirectDraw(IDirect3DDevice9*,const native_mesh_source::DrawRange&){return false;}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_INDEPENDENT_SCENE_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!native_owner_source::enabled||!native_material_source::enabled)return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);
#if defined(_M_IX86)
  if(native_update_source::enabled){enabled=true;ownerThread=GetCurrentThreadId();scene_geometry::observeSelection=Observe;}
#endif
  if(output){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"submit\":false,\"maxLogBytes\":16777216,\"scope\":\"current ordinary inputs before prepare; same-operation comparison\"}\n",enabled?"true":"false");fflush(output);}
  InitializeDirect();
  InitializeSelected();
}
static void EndFrame() {
  DirectEndFrame();
  SelectedEndFrame();
  if(CanLog()&&(scans||compared||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"scans\":%u,\"candidates\":%u,\"selected\":%u,\"dirty\":%u,\"compared\":%u,\"matched\":%u,\"worldDifferences\":%u,\"materialDifferences\":%u,\"resourceCandidates\":%u,\"resourceCompared\":%u,\"resourceMatched\":%u,\"resourceDifferences\":%u,\"resourceRejected\":%u,\"uploadDifferences\":%u,\"drawCandidates\":%u,\"drawCompared\":%u,\"drawMatched\":%u,\"drawDifferences\":%u,\"drawRejected\":%u,\"textureCandidates\":%u,\"textureCompared\":%u,\"textureMatched\":%u,\"textureDifferences\":%u,\"textureRejected\":%u,\"rejected\":[",
      frameId,scans,candidates,selectedCandidates,dirtyCandidates,compared,matched,worldDifferences,materialDifferences,
      resourceCandidates,resourceCompared,resourceMatched,resourceDifferences,resourceRejected,uploadDifferences,
      drawCandidates,drawCompared,drawMatched,drawDifferences,drawRejected,textureCandidates,textureCompared,textureMatched,textureDifferences,textureRejected);
    for(unsigned i=0;i<Count;++i)fprintf(output,"%s%u",i?",":"",rejected[i]);fputs("]}\n",output);fflush(output);
  }
  scans=candidates=selectedCandidates=dirtyCandidates=compared=matched=worldDifferences=materialDifferences=0;memset(rejected,0,sizeof(rejected));
  failureSamples=0;
  resourceCandidates=resourceCompared=resourceMatched=resourceDifferences=resourceRejected=uploadDifferences=0;
  drawCandidates=drawCompared=drawMatched=drawDifferences=drawRejected=0;
  textureCandidates=textureCompared=textureMatched=textureDifferences=textureRejected=0;
}
}
