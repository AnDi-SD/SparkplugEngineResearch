// Own first independent consumer: complete ordinary supports absent from the
// game's original selection. Only our added visibility entries are omitted.
#pragma once
namespace independent_scene_source {
static FILE* submissionOutput;
static bool submissionFailed;
static unsigned directComparisonFrame=UINT32_MAX;
static bool directFrameComparison;
static unsigned directGroups,directInstances,directRejected,directCameraRejected,directQueueRejected;
static unsigned directApiFailures,directUnexpectedDraws,directSuppressedDraws,directTransitions;
static bool DirectCanLog(){return submissionOutput&&_ftelli64(submissionOutput)<16*1024*1024;}
#if defined(_M_IX86)
struct DirectUse {unsigned attempted=0;};
static uint64_t directScope,directRevision;
static unsigned directFrame;
static bool directRunning;
static std::set<uint32_t> directSupports;
static std::map<Key,DirectUse> directUses;
static void BeginDirectScope(){
  if(directComparisonFrame!=frameId){directComparisonFrame=frameId;directFrameComparison=keepForComparison;}
  if(directRunning){submissionFailed=true;++directTransitions;}
  ++directRevision;directScope=0;directSupports.clear();directUses.clear();
}
static bool EmptyQueues(uint32_t renderer) {
  abi::spRendererQueueVectorLayout general{};uint32_t alphaCount=0;uint8_t alphaActive=1;
  if(!Read(renderer+0xc054,general)||!Read(renderer+0x4c,alphaCount)||alphaCount||
     !Read(renderer+0x44,alphaActive)||alphaActive)return false;
  // C050 routes future draws; an allocated empty queue may validly have C050=1.
  if(!general.begin)return !general.end&&!general.capacityEnd;
  return general.begin>=0x10000&&general.begin==general.end&&general.end<=general.capacityEnd&&
    !(general.begin&3)&&!(general.capacityEnd&3)&&(general.capacityEnd-general.begin)%sizeof(abi::spRendererGeneralRecordLayout)==0;
}
static bool DirectCameraNativeCurrent(const scene_geometry::Scope& scope) {
  const auto& camera=native_camera_source::selected;
  if(!native_camera_source::submitEnabled||native_camera_source::submittedFrame!=frameId||
     !camera.valid||camera.frame!=frameId||camera.scene!=scope.scene||camera.camera!=scope.camera||
     camera.deviceEpoch!=native_camera_source::deviceEpoch||!native_camera_source::pending.valid||
     native_camera_source::pending.sequence!=camera.sequence)return false;
  abi::spCameraObservedLayout raw{};
  return Read(scope.camera,raw)&&!memcmp(raw.viewMatrix,camera.info.view,64)&&!memcmp(raw.projectionMatrix,camera.info.projection,64);
}
static bool DirectCameraCurrent(IDirect3DDevice9* device,const scene_geometry::Scope& scope) {
  if(!DirectCameraNativeCurrent(scope))return false;
  IDirect3DSurface9* target=nullptr;
  if(FAILED(device->GetRenderTarget(0,&target))||!target)return false;
  const auto found=primaryTargets.find(device);const bool primary=found!=primaryTargets.end()&&found->second==target;
  target->Release();return primary&&DirectCameraNativeCurrent(scope);
}
// No COM/API calls. This fence is safe after the last external read and before
// DrawInstance. It also rechecks the observer's native phase restrictions.
static bool NativeInputCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,const Input& input) {
  abi::spRendererDrawContextObservedLayout state{};uint8_t mode=1;
  uint32_t selectors[9]{};struct Shader {uint32_t flags[8],selected;} shader{};
  if(!Read(input.owner.renderer+abi::spRendererDrawContextOffset,state)||state.materialOverride||
     state.device!=reinterpret_cast<uintptr_t>(device)||!Read(queueModeAddress,mode)||mode||
     !Read(input.owner.renderer+0xc748,selectors)||!Read(input.owner.renderer+0xe454,shader)||
     shader.selected>=8||shader.flags[shader.selected])return false;
  for(unsigned i=1;i<9;++i)if(selectors[i])return false;
  if(!input.owner.valid||!input.resourcesReady||!input.drawReady||!input.textureWitness.valid||
     !native_owner_source::Current(input.owner)||
     !native_update_source::QualifyWitness(input.witness,input.owner.scene,frameId,input.owner.mutation)||
     !native_transport_source::CurrentTexture(borrow,input.textureWitness)||
     !DefaultStageCurrent(input.owner.renderer,input.defaultStage))return false;
  abi::spRenderSupportObservedLayout support{};abi::spModelLayout model{};
  if(!Read(input.owner.support,support)||!Read(input.owner.model,model)||model.base.base.base.vtableAddress!=0x6eaa58||
     model.baseMeshData!=input.owner.mesh||model.base.material!=input.owner.modelMaterial||model.base.callback2C||model.base.callback30||
     !EmptyCallbacks(model.base.callbacks34)||!EmptyCallbacks(model.base.callbacks44))return false;
  if(support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4)return false;
  unsigned occurrences=0,first=0;
  for(uint32_t i=0;i<(support.renderableEnd-support.renderableBegin)/4;++i) {
    uint32_t address=0;if(!Read(support.renderableBegin+i*4,address))return false;
    if(address==input.owner.model){if(!occurrences)first=i;++occurrences;}
  }
  if(occurrences!=input.owner.modelOccurrences||first!=input.owner.firstModelOrdinal)return false;
  Input current{};current.owner=input.owner;current.witness=input.witness;
  float error=0;uint8_t outer=0;
  if(!WorldInput(current,support)||!MatrixMatches(input.owner.world,current.owner.world,input.computedWorld,error)||
     !MatrixMatches(input.inverse,current.inverse,input.computedWorld,error)||!Read(input.owner.renderer+0xc1c4,outer)||
     !MaterialInput(current,model,uint32_t(reinterpret_cast<uintptr_t>(device)),outer)||
     current.textureCOM!=input.textureCOM||current.effectiveOverride!=input.effectiveOverride||
     current.material.pass!=input.material.pass||current.material.layer!=input.material.layer||
     current.material.textureOwner!=input.material.textureOwner||current.material.texture!=input.material.texture||
     memcmp(current.material.raw,input.material.raw,sizeof(input.material.raw))||
     memcmp(current.materialStates,input.materialStates,sizeof(input.materialStates))||
     !ResourceInput(borrow,device,current))return false;
  const auto& a=current.resources;const auto& b=input.resources;
  return current.cpuGeneration==input.cpuGeneration&&a.vertexGeneration==b.vertexGeneration&&a.indexGeneration==b.indexGeneration&&
    a.declarationGeneration==b.declarationGeneration&&a.vertexBuffer==b.vertexBuffer&&a.indexBuffer==b.indexBuffer&&a.declaration==b.declaration&&
    a.stride==b.stride&&a.componentFlags==b.componentFlags&&!memcmp(&a.range,&b.range,sizeof(a.range))&&
    native_transport_source::CurrentTexture(borrow,input.textureWitness)&&native_owner_source::Current(input.owner)&&
    native_update_source::QualifyWitness(input.witness,input.owner.scene,frameId,input.owner.mutation);
}
static bool DirectNativeCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,const Input& input) {
  return !native_owner_source::activeModel&&!native_owner_source::activeSupport&&NativeInputCurrent(borrow,device,input);
}
static bool DirectCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,const Input& input) {
  return !CompareDrawInput(device,input)&&DirectNativeCurrent(borrow,device,input);
}
struct PreparedDirect {
  Key key{};remixapi_MeshHandle mesh=nullptr;
  Input source{};
  remixapi_InstanceInfoBlendEXT blend{};remixapi_InstanceInfo instance{};
};
template<class Current>
static bool PrepareNativePacket(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                                const Input& input,PreparedDirect& prepared,const Current& current) {
  if(!current())return false;
  native_mesh_source::TransportWitness transport{};native_mesh_source::Geometry geometry{};
  const auto& stamp=input.resources;
  if(!native_transport_source::Witness(borrow,device,stamp.vertexBuffer,stamp.indexBuffer,stamp.declaration,transport)||
     !native_mesh_source::ResolveCurrentResources(borrow,device,input.owner.mesh,input.owner.renderer,input.owner.world,transport,geometry))return false;
  D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH+1]{};const auto count=geometry.layout->size();
  if(!count||count>MAXD3DDECLLENGTH+1)return false;memcpy(layout,geometry.layout->data(),count*sizeof(layout[0]));
  const auto channels=material_channels::UnlitInput(preserveUnlitColor);
  SurfaceGeometryInput conversion{&geometry.vertices->data,&geometry.indices->data,layout,UINT(count),geometry.stride,0,2,
    geometry.range,input.instance.cull,&input.material.contract,&channels};
  std::vector<remixapi_HardcodedVertex> expanded;material_channels::Plan plan{};
  if(!ExpandSurfaceGeometry(conversion,expanded,plan))return false;
  // From here onward only owned expanded vertices and value stamps survive
  // COM/API calls. No borrowed native Bytes/layout pointer is consumed again.
  const auto hash=ChannelTextureHash(input.textureWitness.texture);
  if(!hash||!current()||!native_transport_source::CurrentTexture(borrow,input.textureWitness))return false;
  const auto material=SurfaceChannelMaterial(device,hash,plan,&input.material.sampler,input.textureWitness.texture,&input.textureSrgb);
  if(!material||!current()||!native_transport_source::CurrentTexture(borrow,input.textureWitness))return false;
  prepared.mesh=SurfaceGeometryResource(expanded,material);
  prepared.key={input.owner.support,input.owner.model};prepared.source=input;
  // Same own opaque-alpha policy as the D3D path, applied to the API recipe.
  // Native material values in source remain unchanged for provenance/fences.
  auto instanceState=input.instance;
  if(opaqueAlphaTest&&!keepTrivialAlphaTestForComparison&&
     TrivialOpaqueAlphaTest(instanceState.test,instanceState.function,instanceState.reference,
       instanceState.blendEnabled,instanceState.source,instanceState.destination,instanceState.operation))
    instanceState.function=D3DCMP_ALWAYS;
  return prepared.mesh&&
    DescribeSurfaceInstance(instanceState,input.material.contract,input.owner.world,prepared.mesh,prepared.blend,prepared.instance)&&
    current();
}
static bool PrepareDirect(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,const Input& input,PreparedDirect& prepared) {
  const auto revision=directRevision;
  const auto current=[&](){return !submissionFailed&&revision==directRevision&&
    DirectCurrent(borrow,device,input)&&!submissionFailed&&revision==directRevision;};
  return PrepareNativePacket(borrow,device,input,prepared,current);
}
static bool DirectSupportOmitted(uint32_t support) {
  const auto scope=scene_geometry::active;
  return scope&&scope->serial==directScope&&directFrame==frameId&&directSupports.count(support)!=0;
}
static void DirectSelectionRestarted(uint64_t scope) {
  if(scope!=directScope)return;
  ++directRevision;++directTransitions;submissionFailed=true;
  // A new original selection owns its draws. Previously submitted instances
  // cannot be rolled back through this API; never suppress the new selection.
  directSupports.clear();directUses.clear();
  if(DirectCanLog())fprintf(submissionOutput,"{\"event\":\"unexpected_selection\",\"frame\":%u,\"scope\":%llu}\n",frameId,scope);
}
struct DirectOperation {
  scene_geometry::Scope* scope=nullptr;uint64_t revision=0,serial=0;
  unsigned frame=0;uint32_t manager=0,selectionHeader[3]{};
  std::vector<uint32_t> selection;
  uint64_t cameraSequence=0,cameraEpoch=0,resourceEpoch=0;
};
static bool OperationCurrent(const DirectOperation& operation) {
  if(submissionFailed||!directRunning||directRevision!=operation.revision||frameId!=operation.frame||
     scene_geometry::active!=operation.scope||directScope!=operation.serial||inputScope!=operation.serial||
     native_camera_source::selected.sequence!=operation.cameraSequence||native_camera_source::deviceEpoch!=operation.cameraEpoch)return false;
  uint32_t header[3]{};
  if(!Read(operation.manager+0x2c,header)||memcmp(header,operation.selectionHeader,sizeof(header)))return false;
  for(size_t i=0;i<operation.selection.size();++i) {
    uint32_t support=0;if(!Read(header[0]+i*4,support)||support!=operation.selection[i])return false;
  }
  return true;
}
static bool ModelSequence(uint32_t supportAddress,std::vector<uint32_t>& models) {
  abi::spRenderSupportObservedLayout support{};
  if(!Read(supportAddress,support)||support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4)return false;
  models.resize((support.renderableEnd-support.renderableBegin)/4);
  return models.empty()||scene_geometry::Read(support.renderableBegin,models.data(),models.size()*4);
}
static bool ModelSequenceCurrent(uint32_t supportAddress,const std::vector<uint32_t>& models) {
  abi::spRenderSupportObservedLayout support{};
  if(!Read(supportAddress,support)||support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     support.renderableEnd-support.renderableBegin!=models.size()*4)return false;
  for(size_t i=0;i<models.size();++i) {
    uint32_t model=0;if(!Read(support.renderableBegin+i*4,model)||model!=models[i])return false;
  }
  return true;
}
static bool DirectMeshCurrent(const PreparedDirect& prepared) {
  return SurfaceMeshCurrent(prepared.mesh);
}
static bool GroupCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                         const DirectOperation& operation,uint32_t support,const std::vector<uint32_t>& models,
                         const std::vector<PreparedDirect>& prepared) {
  if(!OperationCurrent(operation)||operation.resourceEpoch!=SurfaceResourceEpoch()||
     !ModelSequenceCurrent(support,models)||prepared.empty()||
     !EmptyQueues(prepared.front().source.owner.renderer)||!DirectCameraNativeCurrent(*operation.scope))return false;
  for(const auto& item:prepared)if(!DirectMeshCurrent(item)||!DirectNativeCurrent(borrow,device,item.source))return false;
  return OperationCurrent(operation);
}
static void SubmitCurrent(const std::unique_lock<std::recursive_mutex>& borrow,scene_geometry::Scope& scope) {
  if(!submitEnabled||keepForComparison||submissionFailed||!materialChannelsEnabled||keepMaterialChannelsForComparison||inputs.empty())return;
  if(directRunning){submissionFailed=true;++directRevision;return;}
  const Input first=inputs.begin()->second;uint32_t deviceAddress=0;
  if(!Read(first.owner.renderer+0xc9e8,deviceAddress))return;
  const auto device=reinterpret_cast<IDirect3DDevice9*>(deviceAddress);
  if(!device)return;
  directScope=scope.serial;directFrame=frameId;directRunning=true;
  struct Running {~Running(){directRunning=false;}} running;
  DirectOperation operation{&scope,directRevision,scope.serial,frameId,inputSelectionManager};
  operation.cameraSequence=native_camera_source::selected.sequence;operation.cameraEpoch=native_camera_source::deviceEpoch;
  if(!Read(operation.manager+0x2c,operation.selectionHeader)||
     !scene_geometry::Vector(operation.manager,0x28,operation.selection)||!OperationCurrent(operation))return;
  if(native_camera_source::submittedFrame!=frameId) {
    native_camera_source::BeforeFirstSceneInstance(device,scope.scene,scope.camera);
    operation.cameraSequence=native_camera_source::selected.sequence;
  }
  if(!DirectCameraCurrent(device,scope)||!OperationCurrent(operation)){++directCameraRejected;return;}
  if(!EmptyQueues(first.owner.renderer)){++directQueueRejected;return;}
  const auto api=GetRemixApi();if(!api||!api->DrawInstance||!OperationCurrent(operation))return;
  // Keys and Inputs used across external calls are owned copies. Reentry may
  // clear the observer's map, so no iterator/reference into it may survive.
  std::set<uint32_t> supports;
  for(const auto& item:inputs) {
    if(!item.second.originallySelected)supports.insert(item.second.owner.support);
  }
  for(const auto supportAddress:supports) {
    if(!OperationCurrent(operation))return;
    if(std::find(operation.selection.begin(),operation.selection.end(),supportAddress)!=operation.selection.end())continue;
    std::vector<uint32_t> models;if(!ModelSequence(supportAddress,models)){++directRejected;continue;}
    std::set<uint32_t> unique(models.begin(),models.end());unique.erase(0);
    bool complete=!unique.empty();std::vector<PreparedDirect> prepared;prepared.reserve(unique.size());
    for(uint32_t model:unique) {
      const auto found=inputs.find({supportAddress,model});
      if(found==inputs.end()||found->second.originallySelected){complete=false;break;}
      const Input source=found->second;
      PreparedDirect next{};const bool ready=PrepareDirect(borrow,device,source,next);
      if(!OperationCurrent(operation))return;
      if(!ready){complete=false;break;}
      prepared.push_back(next);
    }
    if(!complete||prepared.empty()){++directRejected;continue;}
    operation.resourceEpoch=SurfaceResourceEpoch();
    if(!DirectCameraCurrent(device,scope)||CompareDrawInput(device,prepared.front().source)||
       !GroupCurrent(borrow,device,operation,supportAddress,models,prepared)){++directRejected;continue;}
    // Allocate the complete ledger before the first irreversible API draw.
    // A later API error quarantines this support for this frame; old D3D must
    // not duplicate instances already accepted. Following frames use fallback.
    for(const auto& next:prepared)directUses.emplace(next.key,DirectUse{});
    directSupports.insert(supportAddress);++directGroups;
    // Preserve the original vector order, including repeated Models.
    for(unsigned ordinal=0;ordinal<models.size();++ordinal)if(models[ordinal]) {
      auto found=std::find_if(prepared.begin(),prepared.end(),[&](const PreparedDirect& p){return p.key.second==models[ordinal];});
      auto& next=*found;const auto& source=next.source;
      next.instance.pNext=&next.blend;
      {
        if(!GroupCurrent(borrow,device,operation,supportAddress,models,prepared)) {submissionFailed=true;return;}
        const auto occurrence=directUses.find(next.key)->second.attempted;
        ++directUses.find(next.key)->second.attempted;
        const auto result=api->DrawInstance(&next.instance);
        if(result!=REMIXAPI_ERROR_CODE_SUCCESS){++directApiFailures;submissionFailed=true;}
        else ++directInstances;
        if(DirectCanLog()&&(result!=REMIXAPI_ERROR_CODE_SUCCESS||frameId%300==0||triggered))
          fprintf(submissionOutput,"{\"event\":\"instance\",\"frame\":%u,\"scope\":%llu,\"scene\":%u,\"support\":%u,\"model\":%u,\"occurrence\":%u,\"mesh\":%u,\"cpuGeneration\":%llu,\"textureGeneration\":%llu,\"textureContent\":%llu,\"result\":%d,\"originallySelected\":false}\n",
            frameId,scope.serial,scope.scene,supportAddress,next.key.second,occurrence,source.owner.mesh,source.cpuGeneration,
            source.textureWitness.generation,source.textureWitness.contentGeneration,result);
        if(!OperationCurrent(operation)){submissionFailed=true;return;}
        if(!DirectCameraCurrent(device,scope)||CompareDrawInput(device,source)||
           !GroupCurrent(borrow,device,operation,supportAddress,models,prepared)){submissionFailed=true;return;}
      }
    }
  }
}
static bool SkipDirectDraw(IDirect3DDevice9* device,const native_mesh_source::DrawRange& range) {
  const auto scope=native_mesh_source::active;
  if(!scope||!scope->valid||!scope->owner.valid||!DirectSupportOmitted(scope->owner.support))return false;
  // An unexpected native producer is evidence that this cohort was not closed.
  // Forward its original draw; do not infer equivalence from an address/range.
  (void)device;(void)range;++directUnexpectedDraws;submissionFailed=true;
  return false;
}
#endif
static void InitializeDirect() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_INDEPENDENT_SCENE_SUBMIT",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!enabled||!native_camera_source::submitEnabled)return;
#if defined(_M_IX86)
  submissionOutput=_wfsopen(path,L"wb",_SH_DENYNO);if(!submissionOutput)return;
  submitEnabled=true;scene_geometry::skipExtendedSupport=DirectSupportOmitted;scene_geometry::selectionRestarted=DirectSelectionRestarted;
  fprintf(submissionOutput,"{\"event\":\"init\",\"schema\":1,\"enabled\":true,\"scope\":\"complete ordinary supports outside original selection; current native inputs; original selected producers retained\",\"maxLogBytes\":16777216}\n");fflush(submissionOutput);
#endif
}
static void DirectEndFrame() {
  if(DirectCanLog()) {
    fprintf(submissionOutput,"{\"event\":\"frame\",\"frame\":%u,\"groups\":%u,\"instances\":%u,\"rejected\":%u,\"cameraRejected\":%u,\"queueRejected\":%u,\"apiFailures\":%u,\"unexpectedDraws\":%u,\"suppressedDraws\":%u,\"selectionTransitions\":%u,\"disabledAfterFailure\":%s,\"comparison\":%s}\n",
      frameId,directGroups,directInstances,directRejected,directCameraRejected,directQueueRejected,directApiFailures,
      directUnexpectedDraws,directSuppressedDraws,directTransitions,submissionFailed?"true":"false",
      (directComparisonFrame==frameId?directFrameComparison:keepForComparison)?"true":"false");fflush(submissionOutput);
  }
  directGroups=directInstances=directRejected=directCameraRejected=directQueueRejected=directApiFailures=directUnexpectedDraws=directSuppressedDraws=directTransitions=0;
}
}
