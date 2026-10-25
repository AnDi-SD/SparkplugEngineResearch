// Own transition consumer. Native producers run normally; a current before-
// Prepare packet replaces only the final, exactly qualified indexed GPU draw.
#pragma once
namespace independent_scene_source {
static bool selectedSubmitEnabled,keepSelectedForComparison,selectedSubmissionFailed;
static FILE* selectedOutput;
static unsigned selectedAttempts,selectedCalls,selectedInstances,selectedRejected,selectedApiFailures,selectedReentries,selectedPostCommitFaults;
static bool SelectedCanLog(){return selectedOutput&&_ftelli64(selectedOutput)<16*1024*1024;}
#if defined(_M_IX86)
static bool selectedRunning;
static uint64_t selectedRevision;
struct SelectedOperation {
  Input source{};
  native_owner_source::Packet owner{};
  native_mesh_source::DrawRange range{};
  unsigned frame=0,draw=0;
  uint64_t revision=0,sourceRevision=0,cameraSequence=0,cameraEpoch=0;
  d3d9_state_witness::Witness deviceState{};
};
// No COM calls and no borrowed cache pointers survive this fence.
static bool SelectedNativeCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                                  const SelectedOperation& operation) {
  const auto& input=operation.source;const auto scene=scene_geometry::active;
  const auto mesh=native_mesh_source::active;
  if(!selectedRunning||selectedSubmissionFailed||submissionFailed||operation.revision!=selectedRevision||
     operation.sourceRevision!=directRevision||operation.frame!=frameId||operation.draw!=drawId||
     !scene||scene->serial!=inputScope||inputScope!=input.owner.sceneScope||!input.originallySelected||
     !mesh||!mesh->valid||mesh->sequence!=operation.owner.submission||mesh->mesh!=input.owner.mesh||
     mesh->renderer!=input.owner.renderer||mesh->owner.modelCall!=operation.owner.modelCall||
     native_camera_source::selected.sequence!=operation.cameraSequence||native_camera_source::deviceEpoch!=operation.cameraEpoch||
     !DirectCameraNativeCurrent(*scene))return false;
  auto owner=operation.owner;
  if(!native_owner_source::Qualify(owner,input.owner.mesh,input.owner.renderer,owner.submission,owner.world)||
     owner.scene!=input.owner.scene||owner.system!=input.owner.system||owner.root!=input.owner.root||
     owner.support!=input.owner.support||owner.model!=input.owner.model||owner.object!=input.owner.object||
     owner.ownerPrimary!=input.owner.ownerPrimary||owner.modelMaterial!=input.owner.modelMaterial||
     owner.modelOccurrences!=input.owner.modelOccurrences||owner.firstModelOrdinal!=input.owner.firstModelOrdinal||
     owner.selectedMaterial!=input.material.material)return false;
  float error=0;D3DMATRIX rendererWorld{};
  if(!MatrixMatches(input.owner.world,owner.world,input.computedWorld,error)||
     !Read(owner.renderer+0xca40,rendererWorld)||memcmp(&rendererWorld,&owner.world,sizeof(rendererWorld)))return false;
  abi::spRendererDrawContextObservedLayout state{};abi::spDXRendererMaterialCacheObservedLayout installed{};
  if(!Read(owner.renderer+abi::spRendererDrawContextOffset,state)||
     !Read(owner.renderer+abi::spDXRendererMaterialCacheOffset,installed)||
     installed.installedMaterial!=input.material.material||state.selectedMaterial!=input.material.material)return false;
  for(unsigned i=1;i<11;++i)if(state.renderStates[i]!=input.materialStates[i])return false;
  for(unsigned i=1;i<9;++i)if(state.textureStates[i]!=input.material.raw[i])return false;
  uint8_t overrideValue=0;
  if(!Read(owner.renderer+0xc1c4,overrideValue)||overrideValue!=input.effectiveOverride||
     !NativeInputCurrent(borrow,device,input))return false;
  return operation.revision==selectedRevision&&operation.sourceRevision==directRevision&&
    frameId==operation.frame&&drawId==operation.draw;
}
static bool SelectedDeviceCurrent(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                                  SelectedOperation& operation,const ScopedOpaqueAlphaTest* alphaNormalization) {
  if(!SelectedNativeCurrent(borrow,device,operation))return false;
  d3d9_state_witness::Witness before{};
  if(!d3d9_state_witness::Read(borrow,device,before))return false;
  const auto& input=operation.source;const auto& expected=input.resources;
  bool equal=false;
  {
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* declaration=nullptr;
    IDirect3DBaseTexture9* texture=nullptr;IDirect3DVertexShader9* vertexShader=nullptr;IDirect3DPixelShader9* pixelShader=nullptr;
    struct References {
      IDirect3DVertexBuffer9*& vb;IDirect3DIndexBuffer9*& ib;IDirect3DVertexDeclaration9*& declaration;
      IDirect3DBaseTexture9*& texture;IDirect3DVertexShader9*& vs;IDirect3DPixelShader9*& ps;
      ~References(){if(vb)vb->Release();if(ib)ib->Release();if(declaration)declaration->Release();
        if(texture)texture->Release();if(vs)vs->Release();if(ps)ps->Release();}
    } references{vb,ib,declaration,texture,vertexShader,pixelShader};
    const auto inspect=[&](){
      UINT offset=0,stride=0,count=MAXD3DDECLLENGTH+1;D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{};
      if(FAILED(device->GetStreamSource(0,&vb,&offset,&stride))||vb!=expected.vertexBuffer||offset||stride!=expected.stride||
         FAILED(device->GetIndices(&ib))||ib!=expected.indexBuffer||
         FAILED(device->GetVertexDeclaration(&declaration))||declaration!=expected.declaration||
         FAILED(declaration->GetDeclaration(elements,&count))||count>MAXD3DDECLLENGTH+1||
         FAILED(device->GetTexture(0,&texture))||texture!=input.textureWitness.texture||
         FAILED(device->GetVertexShader(&vertexShader))||vertexShader||
         FAILED(device->GetPixelShader(&pixelShader))||pixelShader)return false;
      native_mesh_source::TransportWitness transport{};
      if(!native_transport_source::Witness(borrow,device,vb,ib,declaration,transport)||
         transport.vertexGeneration!=expected.vertexGeneration||transport.indexGeneration!=expected.indexGeneration||
         transport.declarationGeneration!=expected.declarationGeneration||count!=transport.elementCount||
         !transport.elements||memcmp(elements,transport.elements,count*sizeof(elements[0])))return false;
      // Current headers and captured bytes are re-read through the same native
      // resource boundary used before Prepare. Generation alone is not a write witness.
      native_mesh_source::Geometry geometry{};
      if(!native_mesh_source::ResolveCurrentResources(borrow,device,input.owner.mesh,input.owner.renderer,input.owner.world,transport,geometry)||
         memcmp(&geometry.range,&operation.range,sizeof(operation.range))||!geometry.vertices||!geometry.indices||
         geometry.vertices->generation!=input.cpuGeneration||geometry.indices->generation!=input.cpuGeneration||
         !geometry.vertices->verified||!geometry.indices->verified||!native_mesh_source::EqualLayout(geometry,elements,count))return false;
      // The borrowed geometry is not consumed after this point's external calls.
      D3DMATRIX world{},view{},projection{};
      if(FAILED(device->GetTransform(D3DTS_WORLD,&world))||memcmp(&world,&operation.owner.world,sizeof(world))||
         FAILED(device->GetTransform(D3DTS_VIEW,&view))||memcmp(&view,native_camera_source::selected.info.view,sizeof(view))||
         FAILED(device->GetTransform(D3DTS_PROJECTION,&projection))||memcmp(&projection,native_camera_source::selected.info.projection,sizeof(projection)))return false;
      surface_material::Contract actual{};native_material_source::Sampler sampler{};
      if(!surface_material::ReadTexture(device,actual)||!native_material_source::ReadSampler(device,sampler)||
         memcmp(&actual.rgb,&input.material.contract.rgb,sizeof(actual.rgb))||
         memcmp(&actual.alpha,&input.material.contract.alpha,sizeof(actual.alpha))||
         actual.factor!=input.material.contract.factor||actual.coordinates!=input.material.contract.coordinates||
         actual.transformFlags!=input.material.contract.transformFlags||memcmp(&sampler,&input.material.sampler,sizeof(sampler))||
         CompareDrawInput(device,input))return false;
      for(unsigned i=0;i<input.mappedPresent.size();++i)if(input.mappedPresent[i]) {
        DWORD actualState=0;const auto type=static_cast<D3DRENDERSTATETYPE>(i);
        if(FAILED(device->GetRenderState(type,&actualState)))return false;
        DWORD nativeState=actualState;
        if(alphaNormalization)alphaNormalization->RecoverInput(device,type,actualState,nativeState);
        if(nativeState!=input.mappedStates[i])return false;
      }
      if(!SelectedNativeCurrent(borrow,device,operation))return false;
      return DirectCameraCurrent(device,*scene_geometry::active);
    };
    equal=inspect();
  }
  // Includes reference releases; no iterator/raw cache borrow crosses them.
  if(!equal||!SelectedNativeCurrent(borrow,device,operation)||!d3d9_state_witness::Current(borrow,before))return false;
  operation.deviceState=before;return true;
}
static bool TrySelectedDraw(IDirect3DDevice9* device,const native_mesh_source::DrawRange& range,
                            const ScopedOpaqueAlphaTest* alphaNormalization) {
  if(!selectedSubmitEnabled||keepSelectedForComparison||keepForComparison||selectedSubmissionFailed||submissionFailed||
     !submitEnabled||!native_mesh_source::submitEnabled||!native_material_source::submitEnabled||
     !materialChannelsEnabled||keepMaterialChannelsForComparison||GetCurrentThreadId()!=ownerThread)return false;
  if(selectedRunning){++selectedRevision;++selectedReentries;return false;}
  const auto active=native_mesh_source::active;
  if(!active||!active->valid||!active->owner.valid||inputScope!=active->owner.sceneScope)return false;
  const auto found=inputs.find({active->owner.support,active->owner.model});
  if(found==inputs.end()||!found->second.originallySelected)return false;
  ++selectedAttempts;
  SelectedOperation operation{};operation.source=found->second;operation.owner=active->owner;operation.range=range;
  operation.frame=frameId;operation.draw=drawId;operation.revision=++selectedRevision;operation.sourceRevision=directRevision;
  operation.cameraSequence=native_camera_source::selected.sequence;operation.cameraEpoch=native_camera_source::deviceEpoch;
  selectedRunning=true;struct Running {~Running(){selectedRunning=false;}} running;
  std::unique_lock<std::recursive_mutex> borrow(guard);
  bool committed=false,resultReturned=false;
  try {
    const auto current=[&](){return SelectedDeviceCurrent(borrow,device,operation,alphaNormalization);};
    PreparedDirect prepared{};
    if(!PrepareNativePacket(borrow,device,operation.source,prepared,current)){++selectedRejected;return false;}
    const auto api=GetRemixApi();const auto epoch=SurfaceResourceEpoch();
    if(!api||!api->DrawInstance||!current()||epoch!=SurfaceResourceEpoch()||!SurfaceMeshCurrent(prepared.mesh)||
       !SelectedNativeCurrent(borrow,device,operation)||!d3d9_state_witness::Current(borrow,operation.deviceState)){++selectedRejected;return false;}
    prepared.instance.pNext=&prepared.blend;
    // One irrevocable API call owns this intercepted draw from here. An error
    // cannot establish that nothing was enqueued; do not repeat it through D3D.
    committed=true;++selectedCalls;const auto result=api->DrawInstance(&prepared.instance);resultReturned=true;
    if(result==REMIXAPI_ERROR_CODE_SUCCESS)++selectedInstances;
    else ++selectedApiFailures;
    if(!SelectedNativeCurrent(borrow,device,operation)||!d3d9_state_witness::Current(borrow,operation.deviceState)){
      ++selectedPostCommitFaults;selectedSubmissionFailed=true;
    }
    if(result!=REMIXAPI_ERROR_CODE_SUCCESS)selectedSubmissionFailed=true;
    if(SelectedCanLog()&&(result!=REMIXAPI_ERROR_CODE_SUCCESS||frameId%300==0||triggered))
      fprintf(selectedOutput,"{\"event\":\"instance\",\"frame\":%u,\"draw\":%u,\"scope\":%llu,\"scene\":%u,\"support\":%u,\"model\":%u,\"modelCall\":%llu,\"submission\":%llu,\"cpuGeneration\":%llu,\"textureGeneration\":%llu,\"textureContent\":%llu,\"result\":%d}\n",
        operation.frame,operation.draw,operation.source.owner.sceneScope,operation.source.owner.scene,
        operation.source.owner.support,operation.source.owner.model,operation.owner.modelCall,operation.owner.submission,
        operation.source.cpuGeneration,operation.source.textureWitness.generation,operation.source.textureWitness.contentGeneration,result);
    return true;
  }catch(const std::bad_alloc&) {
    if(committed){if(resultReturned)++selectedPostCommitFaults;else ++selectedApiFailures;selectedSubmissionFailed=true;return true;}
    ++selectedRejected;return false;
  }
}
#else
static bool TrySelectedDraw(IDirect3DDevice9*,const native_mesh_source::DrawRange&,const ScopedOpaqueAlphaTest*){return false;}
#endif
static void InitializeSelected() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_SELECTED_SCENE_SUBMIT",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!submitEnabled)return;
  selectedOutput=_wfsopen(path,L"wb",_SH_DENYNO);if(!selectedOutput)return;
  selectedSubmitEnabled=true;
  fprintf(selectedOutput,"{\"event\":\"init\",\"schema\":1,\"enabled\":true,\"scope\":\"before-Prepare native packet; originally selected ordinary Model; commit at qualified indexed draw; client enqueue result only\",\"maxLogBytes\":16777216}\n");fflush(selectedOutput);
}
static void SelectedEndFrame() {
  if(SelectedCanLog()) {
    fprintf(selectedOutput,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"calls\":%u,\"instances\":%u,\"rejected\":%u,\"apiFailures\":%u,\"reentries\":%u,\"postCommitFaults\":%u,\"disabledAfterFailure\":%s}\n",
      frameId,selectedAttempts,selectedCalls,selectedInstances,selectedRejected,selectedApiFailures,selectedReentries,selectedPostCommitFaults,selectedSubmissionFailed?"true":"false");fflush(selectedOutput);
  }
  selectedAttempts=selectedCalls=selectedInstances=selectedRejected=selectedApiFailures=selectedReentries=selectedPostCommitFaults=0;
}
}
