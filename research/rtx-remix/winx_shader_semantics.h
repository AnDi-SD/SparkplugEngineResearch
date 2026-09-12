// Own read-only connection from the original shader manager's key to a sampled
// D3D9 draw. No key decoding, shader replacement or game lighting math here.
namespace shader_semantics {
static FILE* output;
#if defined(_M_IX86)
using NativeSelect=uintptr_t(__thiscall*)(void*,uint32_t,uint32_t);
static NativeSelect originalSelect;
static DWORD ownerThread;
struct Selection { uintptr_t manager=0,object=0; uint32_t mask=0,lights=0; unsigned frame=0; uint64_t sequence=0; };
static thread_local Selection current;
static uint64_t calls,nulls,samples,linked,fixed,unlinked,failures,limited;
static std::map<std::pair<uint32_t,uint32_t>,uint64_t> keys;
static bool Room() {return output && _ftelli64(output)<16*1024*1024;}
static void RecordSelection();
static uintptr_t __fastcall Select(void* manager,void*,uint32_t mask,uint32_t lights) {
  // Let the original implementation complete, including cache hits, generation
  // and its normal NULL path. Observe the actual returned object afterwards.
  const auto object=originalSelect(manager,mask,lights);
  if(GetCurrentThreadId()!=ownerThread)return object;
  current={reinterpret_cast<uintptr_t>(manager),object,mask,lights,frameId,++calls};
  if(!object)++nulls;
  const auto key=std::make_pair(mask,lights);
  try {
    auto found=keys.find(key);
    if(found!=keys.end())++found->second;
    else if(keys.size()<1024){keys.emplace(key,1);if(object)RecordSelection();}
    else ++limited;
  }
  catch(...) {++failures;}
  return object;
}
static bool Verify(IDirect3DVertexShader9* vs,unsigned& shader) {
  uint32_t header[0x54/4]{};
  if(!scene_audit::Read(current.object,header,sizeof(header)) || header[0]!=0x6f2ecc ||
     header[0x50/4]!=reinterpret_cast<uintptr_t>(vs))return false;
  const auto size=header[0x48/4],address=header[0x4c/4];UINT actual=0;
  if(size<8 || size>16384 || size%4 || FAILED(vs->GetFunction(nullptr,&actual)) || actual!=size)return false;
  std::vector<uint8_t> native(size),bound(size);
  if(!scene_audit::Read(address,native.data(),size) || FAILED(vs->GetFunction(bound.data(),&actual)) ||
     actual!=size || native!=bound)return false;
  shader=RememberShader(vs,true);return shader!=unknownShader;
}
static void RecordSelection() {
  if(!Room())return;
  const auto handle=scene_audit::Word(current.object+0x50);unsigned shader=unknownShader;
  if(!handle || !Verify(reinterpret_cast<IDirect3DVertexShader9*>(handle),shader)){++failures;return;}
  fprintf(output,"{\"event\":\"selection\",\"frame\":%u,\"shader\":%u,\"key\":[%u,%u],\"selection\":%llu,\"bytecodeEqual\":true}\n",
    frameId,shader,current.mask,current.lights,current.sequence);fflush(output);
}
static bool Install() {
  // Full EXE hash is checked by Start-Probe. CP38/57 ABI and the complete
  // compared function regions are recorded by qualify_shader_key.py.
  if(!scene_audit::VerifiedImage() || !shaderAuditFile) return false;
  constexpr uint32_t expected[10]={0x4c9660,0x5b7a00,0x4c96e0,0x40ece0,0x4c9480,
    0x408350,0x408370,0x4c8980,0x4c97e0,0x4c8f10};
  uint32_t observed[10]{};
  if(!scene_audit::Read(0x6f2de4,observed,sizeof(observed)) || memcmp(observed,expected,sizeof(expected)))return false;
  constexpr uint8_t prefix[]={0x6a,0xff,0x68,0x01,0x52,0x6c,0x00,0x64,0xa1,0,0,0,0};
  uint8_t bytes[sizeof(prefix)]{};
  if(!scene_audit::Read(0x4c8980,bytes,sizeof(bytes)) || memcmp(bytes,prefix,sizeof(bytes)))return false;
  // One pointer-sized vtable slot, before any rendering. No relocated machine
  // instructions and no extra native-object or COM ownership.
  auto slot=reinterpret_cast<void**>(0x6f2e00);DWORD previous=0,ignored=0;
  if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&previous))return false;
  originalSelect=reinterpret_cast<NativeSelect>(*slot);
  ownerThread=GetCurrentThreadId();
  *slot=reinterpret_cast<void*>(&Select);
  VirtualProtect(slot,sizeof(*slot),previous,&ignored);
  return true;
}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{};
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_SHADER_SEMANTICS",path,MAX_PATH);
  if(!length || length>=MAX_PATH || sizeof(void*)!=4)return;
#if defined(_M_IX86)
  output=_wfsopen(path,L"wb",_SH_DENYNO);if(!output)return;
  const bool installed=Install();
  fprintf(output,"{\"event\":\"init\",\"installed\":%s,\"entry\":%u,\"slot\":%u,\"maxBytes\":16777216,\"maxKeys\":1024}\n",installed?"true":"false",0x4c8980u,0x6f2e00u);
  fflush(output);
  if(!installed){fclose(output);output=nullptr;}
#endif
}
static void Draw(IDirect3DDevice9* device) {
#if defined(_M_IX86)
  if(!Room() || GetCurrentThreadId()!=ownerThread || (frameId>1 && frameId%300 && !triggered && frameId>=traceUntilFrame))return;
  ++samples;
  IDirect3DVertexShader9* vs=nullptr;
  if(FAILED(device->GetVertexShader(&vs))){++failures;return;}
  if(!vs){++fixed;return;}
  struct Release {IDirect3DVertexShader9* value;~Release(){value->Release();}} release{vs};
  // Do not associate an explicit shader or a previous frame by a stale handle.
  // Re-read both the native object and GetFunction on each sampled draw; no
  // permanent COM-pointer -> semantic map survives release/address reuse.
  uint32_t header[0x54/4]{};
  if(current.frame!=frameId || !current.object ||
     !scene_audit::Read(current.object,header,sizeof(header)) || header[0]!=0x6f2ecc ||
     header[0x50/4]!=reinterpret_cast<uintptr_t>(vs)){++unlinked;return;}
  // Refresh identity through bytecode, even when the COM address was reused.
  unsigned shader=unknownShader;
  if(!Verify(vs,shader)){++failures;return;}
  ++linked;
  fprintf(output,"{\"event\":\"draw\",\"frame\":%u,\"draw\":%u,\"shader\":%u,\"key\":[%u,%u],\"selection\":%llu,\"manager\":%u,\"object\":%u,\"bytecodeEqual\":true}\n",
    frameId,drawId,shader,current.mask,current.lights,current.sequence,
    static_cast<unsigned>(current.manager),static_cast<unsigned>(current.object));
#else
  (void)device;
#endif
}
static void EndFrame() {
#if defined(_M_IX86)
  if(!Room() || (frameId%300 && !triggered))return;
  fprintf(output,"{\"event\":\"snapshot\",\"frame\":%u,\"calls\":%llu,\"nulls\":%llu,\"samples\":%llu,\"linked\":%llu,\"fixed\":%llu,\"unlinked\":%llu,\"failures\":%llu,\"limited\":%llu,\"keys\":[",
    frameId,calls,nulls,samples,linked,fixed,unlinked,failures,limited);
  bool first=true;for(const auto& item:keys){fprintf(output,"%s[%u,%u,%llu]",first?"":",",item.first.first,item.first.second,item.second);first=false;}
  fputs("]}\n",output);fflush(output);
#endif
}
} // namespace shader_semantics
