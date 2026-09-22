#pragma once
// Own current draw qualification and opt-in signed Skin submission. Original
// producers run unchanged; one qualified API call owns the intercepted draw.
#include "winx_skin_shader_contract.h"
namespace skin_draw_source {
static FILE* output;
static bool enabled,submitEnabled,submitFailed,running;
static unsigned attempts,qualified,failures;
static unsigned submitAttempts,submitCalls,submitSucceeded,submitRejected,submitFaults;
static unsigned reportedRefusals;
static uint64_t elapsedMilliseconds;
enum class Phase:unsigned {Read,Submit,Packet,State,Shader,Constants,Color,Current,GpuPrepare,Material,Mesh,Instance,CurrentPacket,CurrentLights,Count};
static constexpr const char* phaseNames[]={"read","submit","packet","state","shader","constants","color","current","gpuPrepare","material","mesh","instance","currentPacket","currentLights"};
static std::array<uint64_t,unsigned(Phase::Count)> phaseTicks{},phaseCalls{};
struct Measure {
  Phase phase;LARGE_INTEGER begin{};
  explicit Measure(Phase value):phase(value){QueryPerformanceCounter(&begin);++phaseCalls[unsigned(phase)];}
  ~Measure(){LARGE_INTEGER end{};if(QueryPerformanceCounter(&end)&&end.QuadPart>=begin.QuadPart)phaseTicks[unsigned(phase)]+=uint64_t(end.QuadPart-begin.QuadPart);}
};
template<class Action> static auto Timed(Phase phase,Action&& action)->decltype(action()) {
  Measure measure(phase);return action();
}
static uint64_t Microseconds(uint64_t ticks) {
  static const uint64_t frequency=[](){LARGE_INTEGER value{};return QueryPerformanceFrequency(&value)&&value.QuadPart>0?uint64_t(value.QuadPart):0;}();
  return frequency?(ticks/frequency)*1000000+(ticks%frequency)*1000000/frequency:0;
}
static winx_remix::skin_shader::Catalog catalog;
static bool Enabled(){return enabled;}
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
#if defined(_M_IX86)
namespace packets=winx_remix::skin_packet;
namespace shader=winx_remix::skin_shader;
namespace source=native_mesh_source;
namespace abi=sparkplug::evidence::pc;
enum Reason:unsigned {None,Scope,DeviceState,Packet,Binding,Program,Constant,Palette,Matrix,Camera,Material,Texture,WorldUpdate,Changed,Allocation};
// Snapshot read failures retain skin_draw_state's 1..701 diagnostic codes.
enum ProgramDetail:unsigned {VertexShaderRead=1000,VertexShaderIdentity,PixelShaderBound,
  BoundShaderEvidence,ProgramCatalog,ProgramInfluences};
struct Matrices {
  std::array<float,16> world{},view{},projection{},worldView{},viewProjection{};
  std::array<float,16> uv{};
  bool uvEnabled=false;
  uint8_t dirty=0;
};
struct Operation {
  unsigned frame=0,draw=0;
  native_skin_packet_source::Packet packet;
  shader_semantics::BoundSelection shader;
  winx_remix::skin_draw_state::Snapshot state;
  winx_remix::skin_draw::Color4 prepared;
  std::array<float,1024> constants{};
  native_transport_source::TextureWitness texture{};
  native_camera_source::Packet camera{};
  native_update_source::Witness update{};
  native_instance_lifetime::Token instance{};
  scene_audit::DirectLightWitness lights{};
  d3d9_state_witness::Witness deviceState{};
  Matrices matrices{};
  packets::pc::FixedUvRegistersForAnalysis uvTransform{};
  bool cameraSubmitted=false,lightsSubmitted=false,instanceQualified=false;
};
struct Diagnostic {
  bool stateRead=false;
  std::array<uint32_t,2> observedKey{};
  std::array<DWORD,std::size(winx_remix::skin_draw_state::StageKeys)> stage0{};
};
static bool Fail(unsigned& reason,Reason value){reason=value;return false;}
static bool ReadMatrices(uint32_t renderer,Matrices& result,bool uvEnabled=false) {
  result.uvEnabled=uvEnabled;
  return source::Read(renderer+0xca40,result.world)&&source::Read(renderer+0xca80,result.view)&&
    source::Read(renderer+0xcac0,result.projection)&&source::Read(renderer+0xcb00,result.worldView)&&
    source::Read(renderer+0xcb40,result.viewProjection)&&source::Read(renderer+0xf2f4,result.dirty)&&
    (!uvEnabled||source::Read(renderer+0xf0f4,result.uv));
}
static bool CameraCurrent(const Operation& op) {
  const auto& c=op.camera;const auto& pending=native_camera_source::pending;
  if(!native_camera_source::enabled||!c.valid||c.frame!=frameId||c.deviceEpoch!=native_camera_source::deviceEpoch||
     !pending.valid||pending.sequence!=c.sequence||pending.scene!=c.scene||pending.camera!=c.camera||
     c.scene!=op.packet.stamp.skin.scene||c.camera!=op.packet.stamp.skin.camera)return false;
  abi::spCameraObservedLayout raw{};
  return source::Read(c.camera,raw)&&!memcmp(raw.viewMatrix,c.info.view,64)&&!memcmp(raw.projectionMatrix,c.info.projection,64);
}
static bool CameraSubmitted(const Operation& op) {
  return native_camera_source::submitEnabled&&native_camera_source::submittedFrame==op.frame&&
    native_camera_source::selected.valid&&native_camera_source::selected.sequence==op.camera.sequence;
}
static bool Current(const std::unique_lock<std::recursive_mutex>& borrow,const Operation& op,unsigned* detail=nullptr) {
  Measure measure(Phase::Current);
  const auto changed=[&](unsigned value){if(detail)*detail=value;return false;};
  if(op.frame!=frameId||op.draw!=drawId)return changed(1);
  if(!d3d9_state_witness::Current(borrow,op.deviceState))return changed(2);
  if(!CameraCurrent(op))return changed(3);
  if(!Timed(Phase::CurrentPacket,[&]{return native_skin_packet_source::Current(op.packet.stamp);}))return changed(4);
  if(!shader_semantics::Current(op.shader))return changed(5);
  if(!native_transport_source::CurrentTexture(borrow,op.texture))return changed(6);
  if(!native_update_source::QualifyWitness(op.update,op.packet.stamp.skin.scene,op.frame,op.packet.stamp.skin.owner.mutation))return changed(7);
  if(op.cameraSubmitted&&!CameraSubmitted(op))return changed(9);
  if(op.lightsSubmitted&&!Timed(Phase::CurrentLights,[&]{return scene_audit::CurrentSubmittedDirectLights(borrow,op.lights);}))return changed(10);
  if(op.instanceQualified&&(!native_owner_source::LifetimeInstalled()||!native_instance_lifetime::Current(op.instance)))return changed(11);
  Matrices matrices{};
  const bool valid=ReadMatrices(op.packet.stamp.skin.renderer,matrices,op.matrices.uvEnabled)&&matrices.world==op.matrices.world&&
    matrices.view==op.matrices.view&&matrices.projection==op.matrices.projection&&matrices.worldView==op.matrices.worldView&&
    matrices.viewProjection==op.matrices.viewProjection&&matrices.dirty==op.matrices.dirty&&matrices.uv==op.matrices.uv&&
    op.frame==frameId&&op.draw==drawId&&d3d9_state_witness::Current(borrow,op.deviceState);
  return valid?true:changed(8);
}
static bool Read(IDirect3DDevice9* device,const source::DrawRange& draw,Operation& output,unsigned& reason,unsigned& detail,
    Diagnostic* diagnostic=nullptr) {
  reason=detail=0;
  if(diagnostic)*diagnostic={};
  std::unique_lock<std::recursive_mutex> borrow(guard);
  Operation op;op.frame=frameId;op.draw=drawId;
  if(!native_skin_source::active||!native_skin_source::active->observed)return Fail(reason,Scope);
  if(!d3d9_state_witness::Read(borrow,device,op.deviceState))return Fail(reason,DeviceState);
  if(!Timed(Phase::Packet,[&]{return native_skin_packet_source::CopyDraw(device,draw,op.packet,&detail);}))return Fail(reason,Packet);
  op.camera=native_camera_source::pending;
  if(!CameraCurrent(op))return Fail(reason,Camera);
  if(!native_update_source::GetWitness(op.packet.stamp.skin.scene,op.frame,op.packet.stamp.skin.owner.mutation,op.update))return Fail(reason,WorldUpdate);
  bool accepted=false;
  {
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* declaration=nullptr;
    IDirect3DVertexShader9* vs=nullptr;IDirect3DBaseTexture9* base=nullptr;IDirect3DTexture9* texture=nullptr;IDirect3DSurface9* target=nullptr;
    struct References {
      IDirect3DVertexBuffer9*& vb;IDirect3DIndexBuffer9*& ib;IDirect3DVertexDeclaration9*& declaration;
      IDirect3DVertexShader9*& vs;IDirect3DBaseTexture9*& base;IDirect3DTexture9*& texture;IDirect3DSurface9*& target;
      ~References(){if(target)target->Release();if(texture)texture->Release();if(base)base->Release();if(vs)vs->Release();if(declaration)declaration->Release();if(ib)ib->Release();if(vb)vb->Release();}
    } references{vb,ib,declaration,vs,base,texture,target};
    const auto inspect=[&](){
      UINT offset=0,stride=0;
      const auto& expected=op.packet.stamp.transport;
      if(FAILED(device->GetStreamSource(0,&vb,&offset,&stride))||FAILED(device->GetIndices(&ib))||
         FAILED(device->GetVertexDeclaration(&declaration))||offset||stride!=op.packet.stamp.mesh.vertexStride||
         vb!=expected.vertexBuffer||ib!=expected.indexBuffer||declaration!=expected.declaration)return Fail(reason,Binding);
      if(!Timed(Phase::State,[&]{return winx_remix::skin_draw_state::ReadSnapshot(device,op.state,&detail);}))return Fail(reason,Program);
      if(diagnostic){diagnostic->stateRead=true;diagnostic->stage0=op.state.stages[0];}
      if(FAILED(device->GetVertexShader(&vs))){detail=VertexShaderRead;return Fail(reason,Program);}
      if(!vs||reinterpret_cast<uintptr_t>(vs)!=op.state.vertexShader){detail=VertexShaderIdentity;return Fail(reason,Program);}
      if(op.state.pixelShader){detail=PixelShaderBound;return Fail(reason,Program);}
      if(!Timed(Phase::Shader,[&]{return shader_semantics::ReadBound(vs,op.shader);})){detail=BoundShaderEvidence;return Fail(reason,Program);}
      if(diagnostic)diagnostic->observedKey={op.shader.selection.mask,op.shader.selection.lights};
      const auto program=Timed(Phase::Shader,[&]{return catalog.Match({op.shader.selection.mask,op.shader.selection.lights},op.shader.bytes);});
      if(!program){detail=ProgramCatalog;return Fail(reason,Program);}
      if((program->key[0]&15)!=op.packet.geometry.influences){detail=ProgramInfluences;return Fail(reason,Program);}
      D3DCAPS9 caps{};
      if(FAILED(device->GetDeviceCaps(&caps))||caps.MaxVertexShaderConst<96||
         FAILED(Timed(Phase::Constants,[&]{return device->GetVertexShaderConstantF(0,op.constants.data(),(std::min)(unsigned(caps.MaxVertexShaderConst),256u));})))return Fail(reason,Constant);
      const auto blend=program->Find("BlendMatrices"),vp=program->Find("VPTransform"),diffuse=program->Find("MatDiffuse"),view=program->Find("view_matrix");
      if(!blend||!vp||!diffuse||blend->count<op.packet.geometry.palette.size()*3||
         memcmp(op.constants.data()+blend->first*4,op.packet.geometry.palette.data(),op.packet.geometry.palette.size()*48))return Fail(reason,Palette);
      const auto uv=program->Find("UVTransform");
      if(!ReadMatrices(op.packet.stamp.skin.renderer,op.matrices,uv!=nullptr)||op.matrices.dirty||
         memcmp(op.constants.data()+vp->first*4,op.matrices.viewProjection.data(),64)||
         (view&&memcmp(op.constants.data()+view->first*4,op.matrices.worldView.data(),view->count*16)))return Fail(reason,Matrix);
      if(uv) {
        memcpy(op.uvTransform.data(),op.constants.data()+uv->first*4,sizeof(op.uvTransform));
        // Original type17 copies native rows into the xyz of each register;
        // Fixed's column-major declaration gives them their shader meaning.
        for(unsigned column=0;column<3;++column)
          if(memcmp(op.uvTransform[column].data(),op.matrices.uv.data()+column*4,12)){
            detail=4;return Fail(reason,Matrix);
          }
      }
      // Initial normal policy requires the identity native world matrix. The
      // palette already emits world positions; other normal transforms need a
      // separately proven projection rather than applying world twice.
      for(unsigned i=0;i<16;++i)if(op.matrices.world[i]!=(i%5==0?1.f:0.f)){detail=1;return Fail(reason,Matrix);}
      const auto computed=abi::node_math::Multiply4ForAnalysis(op.matrices.view,op.matrices.projection);
      if(memcmp(computed.data(),op.matrices.viewProjection.data(),64)){detail=2;return Fail(reason,Matrix);}
      if(op.matrices.worldView!=op.matrices.view){detail=3;return Fail(reason,Matrix);}
      D3DMATRIX actualView{},actualProjection{};
      if(FAILED(device->GetTransform(D3DTS_VIEW,&actualView))||FAILED(device->GetTransform(D3DTS_PROJECTION,&actualProjection))||
         memcmp(&actualView,op.camera.info.view,64)||memcmp(&actualProjection,op.camera.info.projection,64)||
         memcmp(op.matrices.view.data(),op.camera.info.view,64)||memcmp(op.matrices.projection.data(),op.camera.info.projection,64)||
         FAILED(device->GetRenderTarget(0,&target))||!target)return Fail(reason,Camera);
      const auto primary=primaryTargets.find(device);
      if(primary==primaryTargets.end()||primary->second!=target)return Fail(reason,Camera);
      packets::pc::FixedSkinVector material{};memcpy(material.data(),op.constants.data()+diffuse->first*4,16);
      winx_remix::skin_draw::Error materialError{};
      if(!Timed(Phase::Color,[&]{return winx_remix::skin_draw::PrepareColor4(op.packet.geometry,material,op.state,op.prepared,&materialError);})){
        detail=unsigned(materialError);return Fail(reason,Material);
      }
      if(FAILED(device->GetTexture(0,&base))||!base||reinterpret_cast<uintptr_t>(base)!=op.state.textures[0]||
         FAILED(base->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(&texture)))||!texture||
         !native_transport_source::BorrowTexture(borrow,device,texture,op.texture))return Fail(reason,Texture);
      return true;
    };
    accepted=inspect();
  }
  if(!accepted)return false;
  if(!Current(borrow,op,&detail))return Fail(reason,Changed);
  op.cameraSubmitted=CameraSubmitted(op);
  op.lightsSubmitted=scene_audit::ReadSubmittedDirectLights(borrow,op.packet.stamp.skin.scene,op.lights);
  const auto& native=op.packet.stamp.skin;
  if(native.supportState.vtable==abi::spRenderNodeSupportVTable&&native_owner_source::LifetimeInstalled())
    op.instanceQualified=native_instance_lifetime::Borrow({native.scene,native.owner.object,native.skin,native.mesh,
      op.camera.deviceEpoch,reinterpret_cast<uintptr_t>(device),native.ownerPrimary},op.instance);
  if(!Current(borrow,op,&detail))return Fail(reason,Changed);
  output=std::move(op);return true;
}
static winx_remix::skin_packet_submit::DrawResult Submit(IDirect3DDevice9* device,const Operation& op,unsigned& detail) {
  namespace gpu=winx_remix::skin_gpu_submit;
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const auto current=[&](){return !submitFailed&&op.cameraSubmitted&&op.lightsSubmitted&&op.instanceQualified&&Current(borrow,op);};
  detail=20;if(!current())return {};
  std::vector<uint32_t> colors;colors.reserve(op.prepared.packet.geometry.vertices.size());
  for(const auto& vertex:op.prepared.packet.geometry.vertices)colors.push_back(vertex.color);
  gpu::Prepared prepared;detail=21;
  if(!Timed(Phase::GpuPrepare,[&]{return gpu::Prepare(op.packet.geometry,colors,op.prepared.state.instance.cull,prepared,
      op.matrices.uvEnabled?&op.uvTransform:nullptr);}))return {};
  remixapi_MaterialHandle material=nullptr;
  {
    IDirect3DBaseTexture9* base=nullptr;IDirect3DTexture9* texture=nullptr;
    struct References {IDirect3DBaseTexture9*& base;IDirect3DTexture9*& texture;
      ~References(){if(texture)texture->Release();if(base)base->Release();}} references{base,texture};
    detail=22;
    if(FAILED(device->GetTexture(0,&base))||!base||reinterpret_cast<uintptr_t>(base)!=op.state.textures[0]||
       FAILED(base->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(&texture)))||
       texture!=op.texture.texture||!current())return {};
    const auto hash=ChannelTextureHash(texture);detail=23;
    if(!hash||!current())return {};
    material=Timed(Phase::Material,[&]{return SurfaceChannelMaterial(device,hash,op.prepared.packet.material,&op.prepared.state.sampler,texture,&op.prepared.state.srgb);});
  }
  detail=24;if(!material||!current())return {};
  const auto mesh=Timed(Phase::Mesh,[&]{return gpu::Resource(prepared,material,op.instance.identity);});
  detail=25;if(!mesh||!current())return {};
  detail=26;const auto result=Timed(Phase::Instance,[&]{return gpu::Draw(mesh,material,prepared,op.prepared.state.instance,op.prepared.state.texture,current);});
  if(result.apiCalled) {
    // Retain the irreversible call result even when post-commit validation
    // cannot allocate. Losing it would incorrectly permit the original draw.
    try {if(!result.apiSucceeded||!result.stateStable||!current()){submitFailed=true;++submitFaults;}}
    catch(const std::bad_alloc&){submitFailed=true;++submitFaults;}
  }
  if(result.apiCalled)detail=0;
  return result;
}
static bool Process(IDirect3DDevice9* device,const source::DrawRange& draw) noexcept {
  if(!enabled||running||!native_skin_source::active||!native_skin_source::active->observed)return false;
  const bool submitting=submitEnabled&&!submitFailed;
  const bool sampled=CanLog()&&(frameId<=1||frameId%300==0||triggered||frameId<traceUntilFrame);
  if(!submitting&&!sampled)return false;
  running=true;struct Running {~Running(){running=false;}} runningScope;
  const auto started=GetTickCount64();++attempts;unsigned reason=0,detail=0;Operation op;
  Diagnostic diagnostic;
  winx_remix::skin_packet_submit::DrawResult submitted;
  bool valid=false;
  try {
    valid=Timed(Phase::Read,[&]{return Read(device,draw,op,reason,detail,&diagnostic);});
    if(valid&&submitting){++submitAttempts;submitted=Timed(Phase::Submit,[&]{return Submit(device,op,detail);});
      if(submitted.apiCalled){++submitCalls;if(submitted.apiSucceeded)++submitSucceeded;}else ++submitRejected;}
  }catch(const std::bad_alloc&){reason=Allocation;}
  if(valid)++qualified;else ++failures;
  elapsedMilliseconds+=GetTickCount64()-started;
  const auto scope=native_skin_source::active;
  // Preserve early refusal reasons between sampled frames without an
  // unbounded per-draw log on unsupported programs or loading transitions.
  const bool firstRefusal=reportedRefusals<32&&(!valid||(submitting&&!submitted.apiCalled));
  if(CanLog()&&(sampled||firstRefusal||submitFailed)) {
    if(firstRefusal)++reportedRefusals;
    fprintf(output,"{\"event\":\"draw\",\"frame\":%u,\"draw\":%u,\"skinCall\":%llu,\"candidate\":%s,\"reason\":%u,\"detail\":%u,\"cameraSubmitted\":%s,\"directLightsSubmitted\":%s,\"instanceQualified\":%s,\"instanceIdentity\":%llu,\"apiCalled\":%s,\"apiSucceeded\":%s,\"vertices\":%zu,\"key\":[%u,%u]",
    frameId,drawId,scope?scope->sequence:0,valid?"true":"false",reason,detail,op.cameraSubmitted?"true":"false",op.lightsSubmitted?"true":"false",
    op.instanceQualified?"true":"false",op.instance.identity,submitted.apiCalled?"true":"false",submitted.apiSucceeded?"true":"false",
    op.packet.geometry.vertices.size(),op.shader.selection.mask,op.shader.selection.lights);
    if(!valid&&diagnostic.stateRead) {
      fprintf(output,",\"observedKey\":[%u,%u],\"stage0\":[",diagnostic.observedKey[0],diagnostic.observedKey[1]);
      for(size_t i=0;i<diagnostic.stage0.size();++i)fprintf(output,"%s[%u,%lu]",i?",":"",unsigned(winx_remix::skin_draw_state::StageKeys[i]),diagnostic.stage0[i]);
      fputs("]",output);
    }
    fputs("}\n",output);
  }
  if(output)fflush(output);
  return submitted.apiCalled;
}
#else
static bool Process(IDirect3DDevice9*,const native_mesh_source::DrawRange&) noexcept {return false;}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{},contracts[MAX_PATH]{};
  const auto size=GetEnvironmentVariableW(L"WINX_REMIX_SKIN_DRAW_AUDIT",path,MAX_PATH);
  if(!size||size>=MAX_PATH||sizeof(void*)!=4)return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);if(!output)return;
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_SKIN_SHADER_CONTRACT",contracts,MAX_PATH);
  if(length&&length<MAX_PATH) {
    FILE* input=_wfsopen(contracts,L"rb",_SH_DENYWR);
    if(input){
      if(!_fseeki64(input,0,SEEK_END)){
        const auto bytes=_ftelli64(input);
        if(bytes>0&&bytes<=winx_remix::skin_shader::MaximumCatalogBytes&&!_fseeki64(input,0,SEEK_SET)){
          try {std::vector<uint8_t> data(static_cast<size_t>(bytes));if(fread(data.data(),1,data.size(),input)==data.size())enabled=catalog.Decode(data);}
          catch(const std::bad_alloc&){}
        }
      }
      fclose(input);
    }
  }
  wchar_t submit[8]{};submitEnabled=enabled&&GetEnvironmentVariableW(L"WINX_REMIX_SKIN_SUBMIT",submit,8)==1&&submit[0]==L'1';
  fprintf(output,"{\"event\":\"init\",\"enabled\":%s,\"submitEnabled\":%s,\"programs\":%zu,\"scope\":\"current shader/geometry/material/camera; opt-in signed API requires direct lights and tracked instance lifetime; API acceptance is not GPU completion\"}\n",enabled?"true":"false",submitEnabled?"true":"false",catalog.programs.size());fflush(output);
}
static void EndFrame() {
  if(CanLog()&&attempts){fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"qualified\":%u,\"failures\":%u,\"milliseconds\":%llu,\"submitAttempts\":%u,\"apiCalled\":%u,\"apiSucceeded\":%u,\"submitRejected\":%u,\"submitFaults\":%u,\"disabledAfterFailure\":%s}\n",
    frameId,attempts,qualified,failures,elapsedMilliseconds,submitAttempts,submitCalls,submitSucceeded,submitRejected,submitFaults,submitFailed?"true":"false");
    fprintf(output,"{\"event\":\"timing\",\"frame\":%u,\"microseconds\":{",frameId);
    for(unsigned i=0;i<unsigned(Phase::Count);++i)fprintf(output,"%s\"%s\":%llu",i?",":"",phaseNames[i],Microseconds(phaseTicks[i]));
    fputs("},\"calls\":{",output);
    for(unsigned i=0;i<unsigned(Phase::Count);++i)fprintf(output,"%s\"%s\":%llu",i?",":"",phaseNames[i],phaseCalls[i]);
    fputs("}}\n",output);fflush(output);}
  attempts=qualified=failures=0;elapsedMilliseconds=0;
  submitAttempts=submitCalls=submitSucceeded=submitRejected=submitFaults=0;
  phaseTicks={};phaseCalls={};
}
} // namespace skin_draw_source
