#pragma once
// Own current draw qualification. Original producers and D3D draw still run.
// This boundary owns snapshots and uses the shared Skin/material preparation.
// A qualified candidate does not by itself authorize a live Remix submission.
#include "winx_skin_shader_contract.h"
namespace skin_draw_source {
static FILE* output;
static bool enabled;
static unsigned attempts,qualified,failures;
static uint64_t elapsedMilliseconds;
static winx_remix::skin_shader::Catalog catalog;
static bool Enabled(){return enabled;}
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
#if defined(_M_IX86)
namespace packets=winx_remix::skin_packet;
namespace shader=winx_remix::skin_shader;
namespace source=native_mesh_source;
namespace abi=sparkplug::evidence::pc;
enum Reason:unsigned {None,Scope,DeviceState,Packet,Binding,Program,Constant,Palette,Matrix,Camera,Material,Texture,WorldUpdate,Changed,Allocation};
struct Matrices {
  std::array<float,16> world{},view{},projection{},worldView{},viewProjection{};
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
  scene_audit::DirectLightWitness lights{};
  d3d9_state_witness::Witness deviceState{};
  Matrices matrices{};
  bool cameraSubmitted=false,lightsSubmitted=false;
};
static bool Fail(unsigned& reason,Reason value){reason=value;return false;}
static bool ReadMatrices(uint32_t renderer,Matrices& result) {
  return source::Read(renderer+0xca40,result.world)&&source::Read(renderer+0xca80,result.view)&&
    source::Read(renderer+0xcac0,result.projection)&&source::Read(renderer+0xcb00,result.worldView)&&
    source::Read(renderer+0xcb40,result.viewProjection)&&source::Read(renderer+0xf2f4,result.dirty);
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
  const auto changed=[&](unsigned value){if(detail)*detail=value;return false;};
  if(op.frame!=frameId||op.draw!=drawId)return changed(1);
  if(!d3d9_state_witness::Current(borrow,op.deviceState))return changed(2);
  if(!CameraCurrent(op))return changed(3);
  if(!native_skin_packet_source::Current(op.packet.stamp))return changed(4);
  if(!shader_semantics::Current(op.shader))return changed(5);
  if(!native_transport_source::CurrentTexture(borrow,op.texture))return changed(6);
  if(!native_update_source::QualifyWitness(op.update,op.packet.stamp.skin.scene,op.frame,op.packet.stamp.skin.owner.mutation))return changed(7);
  if(op.cameraSubmitted&&!CameraSubmitted(op))return changed(9);
  if(op.lightsSubmitted&&!scene_audit::CurrentSubmittedDirectLights(borrow,op.lights))return changed(10);
  Matrices matrices{};
  const bool valid=ReadMatrices(op.packet.stamp.skin.renderer,matrices)&&matrices.world==op.matrices.world&&
    matrices.view==op.matrices.view&&matrices.projection==op.matrices.projection&&matrices.worldView==op.matrices.worldView&&
    matrices.viewProjection==op.matrices.viewProjection&&matrices.dirty==op.matrices.dirty&&
    op.frame==frameId&&op.draw==drawId&&d3d9_state_witness::Current(borrow,op.deviceState);
  return valid?true:changed(8);
}
static bool Read(IDirect3DDevice9* device,const source::DrawRange& draw,Operation& output,unsigned& reason,unsigned& detail) {
  reason=detail=0;
  std::unique_lock<std::recursive_mutex> borrow(guard);
  Operation op;op.frame=frameId;op.draw=drawId;
  if(!native_skin_source::active||!native_skin_source::active->observed)return Fail(reason,Scope);
  if(!d3d9_state_witness::Read(borrow,device,op.deviceState))return Fail(reason,DeviceState);
  if(!native_skin_packet_source::CopyDraw(device,draw,op.packet,&detail))return Fail(reason,Packet);
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
      if(!winx_remix::skin_draw_state::ReadSnapshot(device,op.state,&detail)||FAILED(device->GetVertexShader(&vs))||!vs||
         reinterpret_cast<uintptr_t>(vs)!=op.state.vertexShader||op.state.pixelShader)return Fail(reason,Program);
      if(!shader_semantics::ReadBound(vs,op.shader))return Fail(reason,Program);
      const auto program=catalog.Match({op.shader.selection.mask,op.shader.selection.lights},op.shader.bytes);
      if(!program||(program->key[0]&15)!=op.packet.geometry.influences)return Fail(reason,Program);
      D3DCAPS9 caps{};
      if(FAILED(device->GetDeviceCaps(&caps))||caps.MaxVertexShaderConst<96||
         FAILED(device->GetVertexShaderConstantF(0,op.constants.data(),(std::min)(unsigned(caps.MaxVertexShaderConst),256u))))return Fail(reason,Constant);
      const auto blend=program->Find("BlendMatrices"),vp=program->Find("VPTransform"),diffuse=program->Find("MatDiffuse"),view=program->Find("view_matrix");
      if(!blend||!vp||!diffuse||blend->count<op.packet.geometry.palette.size()*3||
         memcmp(op.constants.data()+blend->first*4,op.packet.geometry.palette.data(),op.packet.geometry.palette.size()*48))return Fail(reason,Palette);
      if(!ReadMatrices(op.packet.stamp.skin.renderer,op.matrices)||op.matrices.dirty||
         memcmp(op.constants.data()+vp->first*4,op.matrices.viewProjection.data(),64)||
         (view&&memcmp(op.constants.data()+view->first*4,op.matrices.worldView.data(),view->count*16)))return Fail(reason,Matrix);
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
      if(!winx_remix::skin_draw::PrepareColor4(op.packet.geometry,material,op.state,op.prepared,&materialError)){
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
  if(!Current(borrow,op,&detail))return Fail(reason,Changed);
  output=std::move(op);return true;
}
static void Observe(IDirect3DDevice9* device,const source::DrawRange& draw) noexcept {
  if(!enabled||!CanLog()||!native_skin_source::active||!native_skin_source::active->observed||
     (frameId>1&&frameId%300&&!triggered&&frameId>=traceUntilFrame))return;
  const auto started=GetTickCount64();++attempts;unsigned reason=0,detail=0;Operation op;
  bool valid=false;
  try {valid=Read(device,draw,op,reason,detail);}catch(const std::bad_alloc&){reason=Allocation;}
  if(valid)++qualified;else ++failures;
  elapsedMilliseconds+=GetTickCount64()-started;
  const auto scope=native_skin_source::active;
  fprintf(output,"{\"event\":\"draw\",\"frame\":%u,\"draw\":%u,\"skinCall\":%llu,\"candidate\":%s,\"reason\":%u,\"detail\":%u,\"cameraSubmitted\":%s,\"directLightsSubmitted\":%s,\"apiCalled\":false,\"vertices\":%zu,\"key\":[%u,%u]}\n",
    frameId,drawId,scope?scope->sequence:0,valid?"true":"false",reason,detail,op.cameraSubmitted?"true":"false",op.lightsSubmitted?"true":"false",op.packet.geometry.vertices.size(),op.shader.selection.mask,op.shader.selection.lights);
  fflush(output);
}
#else
static void Observe(IDirect3DDevice9*,const native_mesh_source::DrawRange&) noexcept {}
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
  fprintf(output,"{\"event\":\"init\",\"enabled\":%s,\"programs\":%zu,\"scope\":\"current shader/geometry/material/camera candidate; observe only; scene light and live API qualification separate\"}\n",enabled?"true":"false",catalog.programs.size());fflush(output);
}
static void EndFrame() {
  if(CanLog()&&attempts){fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"qualified\":%u,\"failures\":%u,\"milliseconds\":%llu}\n",frameId,attempts,qualified,failures,elapsedMilliseconds);fflush(output);}
  attempts=qualified=failures=0;elapsedMilliseconds=0;
}
} // namespace skin_draw_source
