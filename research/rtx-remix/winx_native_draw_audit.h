// Own read-only PC observation, using the shared recovered ABI. No native calls,
// ownership changes, light selection, material evaluation or render overrides.
#include "../../Sparkplug/Analysis/PC/SparkplugAbi.h"
namespace native_draw_audit {
namespace abi = sparkplug::evidence::pc;
static FILE* output;
static DWORD ownerThread;
static std::map<std::string,unsigned> contexts;
static unsigned samples,failures,limited;
static size_t contextBytes;
static bool Room() {return output && _ftelli64(output)<32*1024*1024;}
template<class T> static void Array(std::ostringstream& out,const T* values,size_t n) {
  out<<'[';
  for(size_t i=0;i<n;++i) {
    if(i)out<<',';
    if(std::isfinite(static_cast<double>(values[i])))out<<values[i];else out<<"null";
  }
  out<<']';
}
#if defined(_M_IX86)
template<class T> static bool Read(uintptr_t address,T& value) {
  return scene_audit::Read(address,&value,sizeof(value));
}
static void Light(std::ostringstream& out,uint32_t address) {
  if(!address){out<<"null";return;}
  abi::spLightObservedLayout light{};
  // DXLight is the concrete scene-light family qualified by CP41/43/92.
  const bool valid=Read(address,light) && scene_audit::Word(address)==0x6f0c88 && light.type<=3;
  out<<"{\"address\":"<<address<<",\"valid\":"<<(valid?"true":"false");
  if(valid) {
    out<<",\"type\":"<<light.type<<",\"enabled\":"<<(light.enabled?"true":"false")
       <<",\"projectShadow\":"<<(light.projectShadow?"true":"false")<<",\"rgba\":";
    Array(out,light.colorRGBA,4);out<<",\"intensity\":";Array(out,&light.intensity,1);
    out<<",\"range\":";Array(out,&light.range,1);
    out<<",\"attenuation\":"<<unsigned(light.attenuation);
    out<<",\"position\":";Array(out,light.base.cachedWorldPosition,3);
  }
  out<<'}';
}
static void Passes(std::ostringstream& out,const abi::spMaterialObservedLayout& material) {
  out<<'[';
  for(unsigned i=0;i<material.passCount;++i) {
    if(i)out<<',';
    abi::spMaterialPassLayerObservedLayout pass{};
    const auto address=material.passes[i];
    const bool valid=Read(address,pass) && pass.base.vtableAddress==abi::spMaterialPassLayerVTable && pass.layerCount<=8;
    out<<"{\"address\":"<<address<<",\"valid\":"<<(valid?"true":"false");
    if(valid) {
      out<<",\"blend\":"<<pass.finalBlendOperation<<",\"layers\":[";
      for(unsigned j=0;j<pass.layerCount;++j) {
        if(j)out<<',';
        abi::spMaterialTextureLayerObservedLayout layer{};
        const bool known=Read(pass.layers[j],layer) &&
          (layer.base.vtableAddress==abi::spMaterialTextureLayerVTable || layer.base.vtableAddress==abi::spStdLayerVTable);
        out<<"{\"address\":"<<pass.layers[j]<<",\"vtable\":"<<layer.base.vtableAddress
           <<",\"known\":"<<(known?"true":"false");
        if(known)out<<",\"materialTexture\":"<<layer.materialTexture;
        out<<'}';
      }
      out<<']';
    }
    out<<'}';
  }
  // Multiple passes are a candidate list. No invented current-pass index.
  out<<']';
}
static bool Capture(IDirect3DDevice9* device,std::ostringstream& out) {
  uint32_t renderer=0;
  abi::spRendererDrawContextObservedLayout state{};
  abi::spDXRendererMaterialCacheObservedLayout cached{};
  if(!Read(abi::spRendererSingletonAddress,renderer) ||
     scene_audit::Word(renderer)!=abi::spPCRendererPrimaryVTable ||
     !Read(renderer+abi::spRendererDrawContextOffset,state) || state.device!=reinterpret_cast<uintptr_t>(device) ||
     !Read(renderer+abi::spDXRendererMaterialCacheOffset,cached))return false;
  out<<"{\"renderer\":"<<renderer<<",\"selectedMaterial\":"<<state.selectedMaterial
     <<",\"installedMaterial\":"<<cached.installedMaterial<<",\"effectiveStates\":";
  Array(out,state.renderStates,12);out<<",\"engineTextureStates\":";Array(out,state.textureStates,72);
  out<<",\"override\":"<<unsigned(state.materialOverride)<<",\"overrideTableActive\":"<<unsigned(state.materialState)
     <<",\"renderableARGB\":"<<state.renderableColorARGB<<",\"rendererAmbient\":";Array(out,state.ambientRGBA,4);
  abi::spDXMaterialObservedLayout material{};
  const bool known=Read(state.selectedMaterial,material) && material.base.base.vtableAddress==abi::spDXMaterialPrimaryVTable &&
    material.base.materialVTable==abi::spDXMaterialInterfaceVTable && material.base.passCount<=8;
  out<<",\"materialValid\":"<<(known?"true":"false");
  if(known) {
    out<<",\"sourceStates\":";Array(out,material.base.renderStates,11);
    out<<",\"sourceDiffuse\":";Array(out,material.diffuseRGBA,4);
    out<<",\"sourceAmbient\":";Array(out,material.ambientRGBA,4);
    out<<",\"sourceSpecular\":";Array(out,material.specularRGBA,4);
    out<<",\"sourceEmissive\":";Array(out,material.emissiveRGBA,4);
    out<<",\"sourcePowerBits\":"<<material.specularPowerBits
       <<",\"colorController\":"<<material.base.materialColorController
       <<",\"vertexAlpha\":"<<unsigned(material.base.useVertexAlpha)<<",\"passes\":";
    Passes(out,material.base);
  }
  out<<",\"cachedDiffuse\":";Array(out,cached.diffuseRGBA,4);
  out<<",\"cachedAmbient\":";Array(out,cached.ambientRGBA,4);
  out<<",\"cachedSpecular\":";Array(out,cached.specularRGBA,4);
  out<<",\"cachedEmissive\":";Array(out,cached.emissiveRGBA,4);
  out<<",\"cachedPowerBits\":"<<cached.specularPowerBits
     <<",\"cachedSources\":["<<cached.diffuseSource<<','<<cached.ambientSource<<']';
  D3DMATERIAL9 d3d{};static_assert(sizeof(d3d)==68);
  if(FAILED(device->GetMaterial(&d3d)))return false;
  out<<",\"cachedMaterialEqualsDevice\":"<<(!memcmp(&d3d,cached.diffuseRGBA,sizeof(d3d))?"true":"false");
  abi::spLightCacheObservedLayout lights{};
  const bool valid=state.selectedLightCache && Read(state.selectedLightCache,lights) && lights.lightCount<=8;
  out<<",\"lightCache\":"<<state.selectedLightCache<<",\"lightCacheValid\":"<<(valid?"true":"false");
  if(valid) {
    out<<",\"lights\":[";
    for(unsigned i=0;i<lights.lightCount;++i){if(i)out<<',';Light(out,lights.ordinaryLights[i]);}
    out<<"],\"ambientLight\":";Light(out,lights.ambientLight);
  }
  out<<",\"activeBones\":"<<state.activeBoneCount<<",\"palette\":"<<state.blendPalette
     <<",\"nativeDeclaration\":"<<state.vertexDeclaration<<",\"nativeVB\":"<<state.vertexBuffer
     <<",\"nativeIB\":"<<state.indexBuffer;
  IDirect3DVertexBuffer9* vb=nullptr;UINT offset=0,stride=0;
  if(FAILED(device->GetStreamSource(0,&vb,&offset,&stride)))return false;
  const auto boundVB=reinterpret_cast<uintptr_t>(vb);if(vb)vb->Release();
  abi::spDXVertexBufferLayout nativeVB{};
  const bool knownVB=Read(state.vertexBuffer,nativeVB) && nativeVB.base.vtableAddress==abi::spDXVertexBufferVTable;
  out<<",\"nativeVBValid\":"<<(knownVB?"true":"false")
     <<",\"nativeVBEqualsDevice\":"<<(knownVB && nativeVB.direct3DVertexBuffer==boundVB?"true":"false")
     <<",\"stride\":"<<stride<<",\"streamOffset\":"<<offset;
  if(knownVB)out<<",\"nativeFVF\":"<<nativeVB.fvfCode<<",\"nativeVBBytes\":"<<nativeVB.byteSize;
  IDirect3DVertexDeclaration9* decl=nullptr;
  if(FAILED(device->GetVertexDeclaration(&decl)))return false;
  out<<",\"declaration\":[";
  if(decl) {
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{};UINT count=MAXD3DDECLLENGTH+1;
    const auto hr=decl->GetDeclaration(elements,&count);decl->Release();
    if(FAILED(hr) || count>MAXD3DDECLLENGTH+1)return false;
    for(unsigned i=0;i<count && elements[i].Stream!=0xff;++i) {
      if(i)out<<',';const auto& e=elements[i];
      out<<'['<<e.Stream<<','<<e.Offset<<','<<unsigned(e.Type)<<','<<unsigned(e.Method)<<','<<unsigned(e.Usage)<<','<<unsigned(e.UsageIndex)<<']';
    }
  }
  out<<"]}";return true;
}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{};
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_DRAW_AUDIT",path,MAX_PATH);
  if(!length || length>=MAX_PATH)return;
#if defined(_M_IX86)
  // Launcher checks full debug EXE hash. Runtime also checks renderer, material,
  // pass/light/buffer families and the current D3D device before using fields.
  if(!scene_audit::VerifiedImage())return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);ownerThread=GetCurrentThreadId();
  if(output){fputs("{\"event\":\"init\",\"schema\":1,\"maxBytes\":33554432,\"maxContexts\":4096,\"maxContextBytes\":16777216,\"scope\":\"draw-time borrowed renderer state; no owner/pass identity inferred\"}\n",output);fflush(output);}
#endif
}
static void Draw(IDirect3DDevice9* device,const char* call) {
#if defined(_M_IX86)
  if(!Room() || GetCurrentThreadId()!=ownerThread || (frameId>1 && frameId%300 && !triggered && frameId>=traceUntilFrame))return;
  ++samples;
  try {
    std::ostringstream out;out.precision(9);
    if(!Capture(device,out)){++failures;return;}
    auto key=out.str();auto found=contexts.find(key);
    if(found==contexts.end()) {
      if(contexts.size()>=4096 || contextBytes+key.size()>16*1024*1024){++limited;return;}
      contextBytes+=key.size();const auto id=static_cast<unsigned>(contexts.size()+1);
      found=contexts.emplace(std::move(key),id).first;
      fprintf(output,"{\"event\":\"context\",\"id\":%u,\"frame\":%u,\"draw\":%u,\"data\":%s}\n",id,frameId,drawId,found->first.c_str());
    }
    fprintf(output,"{\"event\":\"draw\",\"frame\":%u,\"draw\":%u,\"context\":%u,\"call\":\"%s\"}\n",frameId,drawId,found->second,call);
  }catch(...){++failures;}
#else
  (void)device;(void)call;
#endif
}
static void EndFrame() {
  if(!Room() || (!samples && !failures && !limited))return;
  fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"samples\":%u,\"failures\":%u,\"limited\":%u,\"contexts\":%zu,\"contextBytes\":%zu}\n",frameId,samples,failures,limited,contexts.size(),contextBytes);
  fflush(output);samples=failures=limited=0;
}
} // namespace native_draw_audit
