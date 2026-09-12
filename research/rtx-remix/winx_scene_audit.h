// Our optional PC diagnostic adapter. It observes the original visibility call;
// it does not implement or override the game's culling rules. CP13/PC ABI only.
// Installed during initial Direct3DCreate9, never injected into a running frame.
#include <cstring>
#include <cstdlib>
#include <cerrno>

static FILE* sceneAuditFile;
static unsigned sceneAuditCalls;
static bool sceneLightsEnabled, keepSceneLightsForComparison;
static float sceneLightGain=10.0f;
static FILE* sceneLightLog;

// Diagnostic gain only: keep the original light values and stable ownership.
// Conversion on the next scene frame updates existing API hashes normally.
static bool SetSceneLightGain(const char* text) {
  if(!text || !*text) return false;
  char* end=nullptr;errno=0;
  const float value=std::strtof(text,&end);
  if(end==text || *end || errno==ERANGE || !std::isfinite(value) || value<0 || value>1000) return false;
  if(value==sceneLightGain) return true;
  const float previous=sceneLightGain;sceneLightGain=value;
  if(sceneLightLog && _ftelli64(sceneLightLog)<16*1024*1024) {
    fprintf(sceneLightLog,"{\"event\":\"light_gain\",\"frame\":%u,\"previous\":%.9g,\"value\":%.9g}\n",frameId,previous,value);
    fflush(sceneLightLog);
  }
  return true;
}

#if defined(_M_IX86)
namespace scene_audit {
using NativeSelect = uintptr_t (__thiscall*)(void*, void*, void*);
static NativeSelect originalSelect;
static DWORD ownerThread;

static bool Read(uintptr_t address, void* data, size_t size) {
  SIZE_T got=0;
  return address>=0x10000 && address<0x7fff0000 && size<=16384 &&
    size<=0x7fff0000-address && ReadProcessMemory(GetCurrentProcess(),
      reinterpret_cast<const void*>(address),data,size,&got) && got==size;
}
static uint32_t Word(uintptr_t address) {
  uint32_t result=0; Read(address,&result,sizeof(result)); return result;
}
static uint32_t At(const unsigned char* bytes, size_t offset) {
  uint32_t result; memcpy(&result,bytes+offset,sizeof(result)); return result;
}
static void Floats(const unsigned char* bytes, size_t offset, unsigned count) {
  fputc('[',sceneAuditFile);
  for(unsigned i=0;i<count;++i) {
    float value; memcpy(&value,bytes+offset+i*4,4);
    if(i) fputc(',',sceneAuditFile);
    if(std::isfinite(value)) fprintf(sceneAuditFile,"%.9g",value);
    else fputs("null",sceneAuditFile);
  }
  fputc(']',sceneAuditFile);
}
static void Camera(uintptr_t camera) {
  unsigned char raw[0x238]{};
  if(!Read(camera,raw,sizeof(raw)) || (At(raw,0)!=0x6ef1e0 &&
      At(raw,0)!=0x6dea20 && At(raw,0)!=0x6dcbc0)) {
    fputs("null",sceneAuditFile); return;
  }
  fprintf(sceneAuditFile,"{\"address\":%u,\"vtable\":%u,\"view\":",camera,At(raw,0));
  Floats(raw,0xcc,16); fputs(",\"projection\":",sceneAuditFile); Floats(raw,0x10c,16);
  fputs(",\"position\":",sceneAuditFile); Floats(raw,0x74,3);
  fputs(",\"forward\":",sceneAuditFile); Floats(raw,0x1a0,3);
  fputs(",\"nearFar\":",sceneAuditFile); Floats(raw,0xbc,2);
  fputs(",\"frustum\":",sceneAuditFile); Floats(raw,0x1c4,24);
  fprintf(sceneAuditFile,",\"viewport\":[%u,%u,%u,%u],\"alternateProjection\":%u}",
    At(raw,0x170),At(raw,0x174),At(raw,0x178),At(raw,0x17c),raw[0x231]);
}

struct LightRecord { uint32_t address; unsigned char raw[0x158]; };
struct LightRegistry { uint32_t manager=0,count=0; bool valid=false; std::vector<LightRecord> lights; };

static LightRegistry ReadLights(uintptr_t scene) {
  // CP8/CP92: complete Light pointers, borrowed links B8/BC, owned manager34.
  // Read the scene registry, not the eight per-object D3D light slots.
  const auto manager=Word(scene+0x34);
  unsigned char header[0x24]{};
  if(!Read(manager,header,sizeof(header)) || At(header,0)!=0x6e8ca4 ||
      At(header,0x20)!=scene || At(header,0x18)>512) {
    return {};
  }
  LightRegistry registry; registry.manager=manager;
  auto& lights=registry.lights;
  const auto count=At(header,0x18);
  registry.count=count;
  uint32_t pointer=At(header,0x10), previous=0;
  std::set<uint32_t> visited;
  bool valid=true;
  for(unsigned i=0;i<count;++i) {
    LightRecord record{pointer,{}};
    if(!pointer || !visited.insert(pointer).second || !Read(pointer,record.raw,sizeof(record.raw)) ||
        At(record.raw,0)!=0x6f0c88 || At(record.raw,0x3c)!=scene || At(record.raw,0xb8)!=previous) {
      valid=false; break;
    }
    previous=pointer; pointer=At(record.raw,0xbc); lights.push_back(record);
  }
  valid=valid && !pointer && previous==At(header,0x14) && lights.size()==count;
  registry.valid=valid; return registry;
}

static void Lights(uintptr_t scene) {
  const auto registry=ReadLights(scene); const auto& lights=registry.lights;
  fprintf(sceneAuditFile,"{\"manager\":%u,\"valid\":%s,\"count\":%u,\"lights\":[",registry.manager,registry.valid?"true":"false",registry.count);
  for(size_t i=0;i<lights.size();++i) {
    if(i) fputc(',',sceneAuditFile);
    const auto& light=lights[i]; const auto raw=light.raw;
    fprintf(sceneAuditFile,"{\"address\":%u,\"type\":%u,\"flags\":%u,\"enabled\":%s,\"projectShadow\":%s,\"position\":",light.address,At(raw,0xc0),At(raw,0xb0),raw[0xed]?"true":"false",raw[0xec]?"true":"false");
    Floats(raw,0x74,3); fputs(",\"direction\":",sceneAuditFile); Floats(raw,0xa4,3);
    fputs(",\"rgba\":",sceneAuditFile); Floats(raw,0xc4,4);
    fputs(",\"intensity\":",sceneAuditFile); Floats(raw,0xd8,1);
    fputs(",\"rangeAngles\":",sceneAuditFile); Floats(raw,0xe0,3);
    fprintf(sceneAuditFile,",\"attenuation\":%u",raw[0xd4]);
    // Ambient has no initialized D3DLIGHT9. Never serialize its allocator tail.
    if(At(raw,0xc0)<=2) {
      fprintf(sceneAuditFile,",\"deviceType\":%u,\"deviceDiffuse\":",At(raw,0xf0));
      Floats(raw,0xf4,4); fputs(",\"deviceDirection\":",sceneAuditFile); Floats(raw,0x130,3);
      fputs(",\"deviceAttenuation\":",sceneAuditFile); Floats(raw,0x144,3);
    }
    fputc('}',sceneAuditFile);
  }
  fputs("]}",sceneAuditFile);
}

static void SyncLights(uintptr_t scene);

static uintptr_t __fastcall Select(void* manager, void*, void* scene, void* camera) {
  // No catches around the game, no replacement of its return value or output.
  const uintptr_t result=originalSelect(manager,scene,camera);
  if(GetCurrentThreadId()!=ownerThread) return result;
  std::lock_guard<std::recursive_mutex> lock(guard);
  unsigned char header[0x54]{};
  if(!Read(reinterpret_cast<uintptr_t>(manager),header,sizeof(header)) || At(header,0)!=0x6e8cdc) return result;
  const auto input=reinterpret_cast<uintptr_t>(camera), overrideCamera=At(header,0x38);
  const auto effective=overrideCamera?overrideCamera:input;
  const auto engine=Word(0x755274), main=engine?Word(engine+0x1c):0;
  // Diagnostic sampling never gates the operational scene registry.
  if(!sceneAuditFile || !(frameId<2 || frameId%300==0 || frameId<traceUntilFrame)) return result;
  if(_ftelli64(sceneAuditFile)>=64*1024*1024) return result;
  const auto begin=At(header,0x2c), end=At(header,0x30), capacity=At(header,0x34);
  const bool valid=begin<=end && end<=capacity && (end-begin)%4==0 && end-begin<=4096*4;
  uint32_t pointers[4096]{};
  const bool readable=valid && (begin==end || Read(begin,pointers,end-begin));
  const unsigned count=readable?(end-begin)/4:0;
  fprintf(sceneAuditFile,"{\"event\":\"visibility\",\"call\":%u,\"frame\":%u,\"afterDraw\":%u,\"manager\":%u,\"scene\":%u,\"scenePublished\":%u,\"stamp\":%u,\"inputCamera\":%u,\"overrideCamera\":%u,\"mainCamera\":%u,\"isMain\":%s,\"sphereCulling\":%s,\"camera\":",
    ++sceneAuditCalls,frameId,drawId,reinterpret_cast<uintptr_t>(manager),reinterpret_cast<uintptr_t>(scene),At(header,0x40),At(header,0x14),input,overrideCamera,main,effective==main?"true":"false",header[0x3c]?"true":"false");
  Camera(effective);
  fprintf(sceneAuditFile,",\"vectorValid\":%s,\"count\":%u,\"supports\":[",readable?"true":"false",count);
  for(unsigned i=0;i<count;++i) {
    if(i) fputc(',',sceneAuditFile);
    if(!pointers[i]) { fputs("null",sceneAuditFile); continue; }
    unsigned char raw[0x74]{};
    fprintf(sceneAuditFile,"{\"address\":%u",pointers[i]);
    if(Read(pointers[i],raw,sizeof(raw))) {
      fprintf(sceneAuditFile,",\"vtable\":%u,\"completeObject\":%u,\"stamp\":%u,\"worldSphere\":",At(raw,0),At(raw,0x70),At(raw,0x64));
      Floats(raw,0x24,4);
    } else fputs(",\"readError\":true",sceneAuditFile);
    fputc('}',sceneAuditFile);
  }
  fputc(']',sceneAuditFile);
  if(effective==main) {
    fputs(",\"sceneLights\":",sceneAuditFile); Lights(reinterpret_cast<uintptr_t>(scene));
  }
  fputs("}\n",sceneAuditFile); fflush(sceneAuditFile);
  return result;
}

static bool VerifiedImage() {
  wchar_t executable[MAX_PATH]{}, adapter[MAX_PATH]{};
  if(!GetModuleFileNameW(nullptr,executable,MAX_PATH) ||
     !GetModuleFileNameW(selfModule,adapter,MAX_PATH)) return false;
  auto slash=wcsrchr(adapter,L'\\'); if(!slash) return false;
  *(slash+1)=0; wcscat_s(adapter,L"WinxClubDebug.exe");
  // Launcher additionally checks the full documented SHA256 before launch.
  if(_wcsicmp(executable,adapter)!=0 || GetModuleHandleW(nullptr)!=reinterpret_cast<HMODULE>(0x400000) || frameId || drawId) return false;
  // Two complete position-independent instructions: push -1; mov eax,fs:[0].
  // CP13 disassembly proves the thiscall(scene,camera), ret 8 convention.
  constexpr unsigned char expected[8]={0x6a,0xff,0x64,0xa1,0,0,0,0};
  unsigned char observed[8]{};
  if(!Read(0x46d270,observed,8) || memcmp(observed,expected,8)) return false;
  return true;
}
static bool Install() {
  if(!VerifiedImage()) return false;
  constexpr unsigned char expected[8]={0x6a,0xff,0x64,0xa1,0,0,0,0};
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,13,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  if(!trampoline) return false;
  memcpy(trampoline,expected,8); trampoline[8]=0xe9;
  auto jump=uint32_t(0x46d278-reinterpret_cast<uintptr_t>(trampoline+13));
  memcpy(trampoline+9,&jump,4);
  DWORD previous=0, ignored=0;
  if(!VirtualProtect(trampoline,13,PAGE_EXECUTE_READ,&previous)) { VirtualFree(trampoline,0,MEM_RELEASE); return false; }
  FlushInstructionCache(GetCurrentProcess(),trampoline,13);
  auto entry=reinterpret_cast<unsigned char*>(0x46d270);
  if(!VirtualProtect(entry,8,PAGE_EXECUTE_READWRITE,&previous)) { VirtualFree(trampoline,0,MEM_RELEASE); return false; }
  originalSelect=reinterpret_cast<NativeSelect>(trampoline); ownerThread=GetCurrentThreadId();
  unsigned char replacement[8]={0xe9,0,0,0,0,0x90,0x90,0x90};
  jump=uint32_t(reinterpret_cast<uintptr_t>(&Select)-0x46d275);
  memcpy(replacement+1,&jump,4); memcpy(entry,replacement,8);
  VirtualProtect(entry,8,previous,&ignored); FlushInstructionCache(GetCurrentProcess(),entry,8);
  return true;
}
} // namespace scene_audit
#endif

static void InitializeSceneAudit() {
  wchar_t path[MAX_PATH]{}, option[8]{}, gain[32]{};
  sceneLightsEnabled=sizeof(void*)==4 && GetEnvironmentVariableW(L"WINX_REMIX_SCENE_LIGHTS",option,8) && wcscmp(option,L"1")==0;
  if(GetEnvironmentVariableW(L"WINX_REMIX_LIGHT_GAIN",gain,32)) {
    const auto value=wcstod(gain,nullptr);
    if(std::isfinite(value) && value>=0 && value<=1000) sceneLightGain=static_cast<float>(value);
  }
  const DWORD length=GetEnvironmentVariableW(L"WINX_REMIX_SCENE_AUDIT",path,MAX_PATH);
  if((!length || length>=MAX_PATH) && !sceneLightsEnabled) return;
#if defined(_M_IX86)
  if(length && length<MAX_PATH) sceneAuditFile=_wfsopen(path,L"wb",_SH_DENYNO);
  if(sceneLightsEnabled && GetEnvironmentVariableW(L"WINX_REMIX_LIGHT_AUDIT",path,MAX_PATH))
    sceneLightLog=_wfsopen(path,L"wb",_SH_DENYNO);
  const bool verified=scene_audit::VerifiedImage();
  const bool installed=sceneAuditFile && verified && scene_audit::Install();
  sceneLightsEnabled=sceneLightsEnabled && verified;
  for(auto output:{sceneAuditFile,sceneLightLog}) if(output) {
    fprintf(output,"{\"event\":\"init\",\"installed\":%s,\"sceneLights\":%s,\"pid\":%lu,\"thread\":%lu,\"entry\":4641392,\"maxBytes\":67108864}\n",installed?"true":"false",sceneLightsEnabled?"true":"false",GetCurrentProcessId(),GetCurrentThreadId());
    fflush(output);
  }
#endif
}

#include "winx_scene_lights.h"
