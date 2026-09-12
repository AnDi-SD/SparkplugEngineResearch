// Own native material snapshot/qualification. Recovered texture mapping is
// shared with spDXRenderer; this file neither executes nor duplicates game logic.
#pragma once
#include "../../Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h"
namespace native_material_source {
namespace abi=sparkplug::evidence::pc;
static bool enabled,submitEnabled;
static FILE* output;
static unsigned attempts,matched,used,mismatches;
enum Reason : unsigned { Scope,Mode,Material,Pass,Layer,Texture,Override,Coordinates,Arguments,Mapping,Device,Count };
static unsigned rejected[Count]{};
static bool Reject(Reason reason) {++rejected[reason];return false;}
struct Sampler {DWORD u=0,v=0,mag=0,min=0,mip=0;};
struct Packet {
  surface_material::Contract contract{};
  material_channels::Input channels{};
  Sampler sampler{};
  uint32_t renderer=0,material=0,pass=0,layer=0,textureOwner=0,texture=0;
  uint32_t raw[9]{};
};
struct Cache {
  std::array<uint32_t,9> raw{};
  uint32_t coordinateIndex=~0u,transformFlags=~0u;
};
struct Mapped {
  std::array<uint32_t,29> stage{};
  std::array<uint32_t,14> sampler{};
  bool valid=true;
  Mapped(){stage.fill(~0u);sampler.fill(~0u);}
};
static std::int32_t Map(void* context,bool sampler,uint32_t stage,uint32_t index,uint32_t value) noexcept {
  auto& out=*static_cast<Mapped*>(context);
  if(stage||index>=(sampler?out.sampler.size():out.stage.size())){out.valid=false;return -1;}
  if(sampler)out.sampler[index]=value;else out.stage[index]=value;
  return 0;
}
static bool ReadSampler(IDirect3DDevice9* d,Sampler& out) {
  return SUCCEEDED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&out.u))&&
    SUCCEEDED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&out.v))&&
    SUCCEEDED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&out.mag))&&
    SUCCEEDED(d->GetSamplerState(0,D3DSAMP_MINFILTER,&out.min))&&
    SUCCEEDED(d->GetSamplerState(0,D3DSAMP_MIPFILTER,&out.mip));
}
#if defined(_M_IX86)
template<class T> static bool Read(uintptr_t address,T& value){return native_mesh_source::Read(address,value);}
static bool Resolve(IDirect3DDevice9* d,const native_mesh_source::Geometry& geometry,
                    const surface_material::Contract& observed,const surface_material::ObservedStage& stage,
                    const material_channels::Input* channels,bool preserveUnlit,Packet& out) {
  if(!enabled)return false;
  ++attempts;
  if(!channels||!geometry.renderer||!native_mesh_source::active||!native_mesh_source::active->valid||
     native_mesh_source::active->renderer!=geometry.renderer||native_mesh_source::active->mesh!=geometry.mesh||
     native_mesh_source::active->sequence!=geometry.submission||native_mesh_source::ownerThread!=GetCurrentThreadId())return Reject(Scope);
  abi::spRendererDrawContextObservedLayout state{};
  abi::spDXRendererMaterialCacheObservedLayout installed{};
  abi::spDXMaterialObservedLayout material{};
  if(!Read(geometry.renderer+abi::spRendererDrawContextOffset,state)||state.device!=reinterpret_cast<uintptr_t>(d)||
     !Read(geometry.renderer+abi::spDXRendererMaterialCacheOffset,installed)||installed.installedMaterial!=state.selectedMaterial||
     !Read(state.selectedMaterial,material)||material.base.base.vtableAddress!=abi::spDXMaterialPrimaryVTable||
     material.base.materialVTable!=abi::spDXMaterialInterfaceVTable)return Reject(Material);
  // CP32 mode2 disables lighting/specular; the existing unlit policy therefore
  // does not consume the source material colors or the ambient selectors.
  if(state.renderStates[8]!=2||material.base.renderStates[8]!=2)return Reject(Mode);
  abi::spMaterialPassLayerObservedLayout pass{};
  if(material.base.passCount!=1||!Read(material.base.passes[0],pass)||
     pass.base.vtableAddress!=abi::spMaterialPassLayerVTable||pass.layerCount!=1)return Reject(Pass);
  abi::spStdLayerObservedLayout layer{};abi::spMaterialTextureObservedLayout texture{};
  if(!Read(pass.layers[0],layer)||layer.base.base.vtableAddress!=abi::spStdLayerVTable||
     !Read(layer.base.materialTexture,texture)||texture.base.vtableAddress!=abi::spMaterialTextureVTable)return Reject(Layer);
  // CP36 uses the selectors even when C1C4 is clear. Read stage0 selectors
  // explicitly; a global override flag is not proof of their chosen source.
  // C1C4 may be set: the only consumed lighting mode is independently matched
  // above, and all other render/blend states still come from D3D.
  std::array<uint32_t,9> selectors{};
  struct ShaderSelection {uint32_t flags[8],selected;} shader{};
  if(state.materialOverride||!Read(geometry.renderer+0xC748,selectors)||
     !Read(geometry.renderer+0xE454,shader)||shader.selected>=8||shader.flags[shader.selected])return Reject(Override);
  for(unsigned index=1;index<9;++index)
    if(selectors[index]||state.textureStates[index]!=texture.textureStates[index])return Reject(Override);
  abi::spDXTextureObservedLayout nativeTexture{};
  if(!Read(texture.fallbackTexture,nativeTexture)||nativeTexture.base.base.base.base.vtableAddress!=abi::spDXTexturePrimaryVTable||
     nativeTexture.device!=reinterpret_cast<uintptr_t>(d)||!nativeTexture.texture)return Reject(Texture);
  IDirect3DBaseTexture9* bound=nullptr;
  const auto textureResult=d->GetTexture(0,&bound);const auto boundAddress=reinterpret_cast<uintptr_t>(bound);
  if(bound)bound->Release();
  if(FAILED(textureResult)||boundAddress!=nativeTexture.texture){++mismatches;return Reject(Texture);}
  // These arguments originate in platform device state, not textureStates[9].
  // The narrow cohort proves TFACTOR unused and retains the existing RESULTARG
  // and disabled-next-stage guards. No invalid native device cache is trusted.
  if(stage.r1!=D3DTA_TEXTURE||stage.r2!=D3DTA_CURRENT||stage.a1!=D3DTA_TEXTURE||stage.a2!=D3DTA_CURRENT||
     stage.result!=D3DTA_CURRENT||stage.next!=D3DTOP_DISABLE)return Reject(Arguments);
  Cache cache{};Mapped mapped;
  for(unsigned index=1;index<9;++index)
    if(!sparkplug::reconstruction::ApplyPCTextureStateForAnalysis(cache,0,index,texture.textureStates[index],false,Map,&mapped))return Reject(Mapping);
  if(!mapped.valid)return Reject(Mapping);
  // First cohort: original UV0 without a transform. Animated/other UV paths
  // remain observable D3D until their full transform source is qualified.
  if(mapped.stage[11]||mapped.stage[24])return Reject(Coordinates);
  out={};out.contract=observed;out.contract.coordinates=mapped.stage[11];out.contract.transformFlags=mapped.stage[24];
  if(!surface_material::Decode(mapped.stage[1],stage.r1,stage.r2,out.contract.rgb)||
     !surface_material::DecodeAlpha(mapped.stage[4],stage.a1,stage.a2,out.contract.alpha))return Reject(Mapping);
  out.sampler={mapped.sampler[1],mapped.sampler[2],mapped.sampler[5],mapped.sampler[6],mapped.sampler[7]};
  out.channels=material_channels::UnlitInput(preserveUnlit);
  DWORD lighting=1,specular=1;Sampler actual{};
  if(FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting))||FAILED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular))||
     !ReadSampler(d,actual))return Reject(Device);
  if(lighting||specular||mapped.stage[1]!=stage.rgb||mapped.stage[4]!=stage.alpha||
     out.contract.coordinates!=observed.coordinates||out.contract.transformFlags!=observed.transformFlags||
     memcmp(&out.sampler,&actual,sizeof(actual))||memcmp(&out.channels,channels,sizeof(*channels))) {
    ++mismatches;return Reject(Device);
  }
  out.renderer=geometry.renderer;out.material=state.selectedMaterial;out.pass=material.base.passes[0];
  out.layer=pass.layers[0];out.textureOwner=layer.base.materialTexture;out.texture=texture.fallbackTexture;
  memcpy(out.raw,texture.textureStates,sizeof(out.raw));++matched;return true;
}
#else
static bool Resolve(IDirect3DDevice9*,const native_mesh_source::Geometry&,const surface_material::Contract&,
                    const surface_material::ObservedStage&,const material_channels::Input*,bool,Packet&){return false;}
#endif
static void RecordUse(const native_mesh_source::Geometry& geometry,const Packet& packet) {
  ++used;
  if(output&&_ftelli64(output)<16*1024*1024&&(frameId%300==0||triggered))
    fprintf(output,"{\"event\":\"submit\",\"frame\":%u,\"draw\":%u,\"mesh\":%u,\"material\":%u,\"pass\":%u,\"layer\":%u,\"textureOwner\":%u,\"texture\":%u,\"rgbOperation\":%u,\"alphaOperation\":%u,\"coordinates\":%u,\"transformFlags\":%u,\"sampler\":[%u,%u,%u,%u,%u],\"source\":\"native_std_layer_shared_mapping\",\"arguments\":\"D3D_guard\"}\n",
      frameId,drawId,geometry.mesh,packet.material,packet.pass,packet.layer,packet.textureOwner,packet.texture,
      packet.raw[1],packet.raw[2],packet.contract.coordinates,packet.contract.transformFlags,
      packet.sampler.u,packet.sampler.v,packet.sampler.mag,packet.sampler.min,packet.sampler.mip);
}
static void Initialize() {
  wchar_t path[MAX_PATH]{},option[8]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_MATERIAL_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!native_mesh_source::enabled)return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);enabled=true;
  submitEnabled=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_MATERIAL_SUBMIT",option,8)&&wcscmp(option,L"1")==0;
  if(output){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":true,\"submit\":%s,\"maxLogBytes\":16777216,\"scope\":\"ordinary single StdLayer mode2 UV0 untransformed; D3D argument and state guards\"}\n",submitEnabled?"true":"false");fflush(output);}
}
static void EndFrame() {
  if(output&&_ftelli64(output)<16*1024*1024&&(attempts||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"matched\":%u,\"used\":%u,\"mismatches\":%u,\"rejected\":[",frameId,attempts,matched,used,mismatches);
    for(unsigned i=0;i<Count;++i)fprintf(output,"%s%u",i?",":"",rejected[i]);
    fputs("]}\n",output);fflush(output);
  }
  attempts=matched=used=mismatches=0;memset(rejected,0,sizeof(rejected));
}
}
