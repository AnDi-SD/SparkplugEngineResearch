// Own native/API boundary. The original camera computes both matrices; this
// module only copies its successful apply result and schedules the WORLD API.
#pragma once
#include "../../Sparkplug/Analysis/PC/SparkplugAbi.h"
namespace native_camera_source {
namespace abi=sparkplug::evidence::pc;
struct Packet {
  remixapi_CameraInfo info{};
  uint32_t scene=0,camera=0,frame=0,afterDraw=0;
  uint64_t sequence=0;
  uint64_t deviceEpoch=0;
  bool valid=false;
};
static Packet pending,selected;
static FILE* output;
static bool enabled,submitEnabled;
static uint32_t observedFrame=UINT32_MAX,submittedFrame=UINT32_MAX,attemptedFrame=UINT32_MAX;
static uint64_t sequence,deviceEpoch;
static unsigned applies,foreignApplies,rejected,matched,submitted,failures,stateMismatches,lateUpdates,missedFirstDraws;
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
static bool KnownCamera(uint32_t table){return table==0x6ef1e0||table==0x6dea20||table==0x6dcbc0;}
static bool Capture(uint32_t scene,uint32_t camera,uint32_t main,uint32_t table,const abi::spCameraObservedLayout& raw) {
  if(!camera||camera!=main){++foreignApplies;return false;}
  if(!scene){pending.valid=false;++rejected;return false;}
  if(!KnownCamera(table)||raw.projectionBranch||raw.twoDimensional||raw.projectionMatrix[11]!=1.0f||raw.projectionMatrix[15]!=0.0f) {
    pending.valid=false;++rejected;return false;
  }
  for(float value:raw.viewMatrix)if(!std::isfinite(value)){pending.valid=false;++rejected;return false;}
  for(float value:raw.projectionMatrix)if(!std::isfinite(value)){pending.valid=false;++rejected;return false;}
  Packet next{};next.scene=scene;next.camera=camera;next.frame=frameId;next.afterDraw=drawId;
  next.sequence=++sequence;next.deviceEpoch=deviceEpoch;next.valid=true;
  next.info.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;next.info.type=REMIXAPI_CAMERA_TYPE_WORLD;
  memcpy(next.info.view,raw.viewMatrix,sizeof(next.info.view));
  memcpy(next.info.projection,raw.projectionMatrix,sizeof(next.info.projection));
  if(observedFrame==frameId&&selected.valid&&
     (next.scene!=selected.scene||next.camera!=selected.camera||memcmp(next.info.view,selected.info.view,128)))++lateUpdates;
  pending=next;++applies;return true;
}
static void LogPacket(remixapi_ErrorCode result,bool native) {
  if(!CanLog())return;
  fprintf(output,"{\"event\":\"camera\",\"frame\":%u,\"beforeDraw\":%u,\"scene\":%u,\"camera\":%u,\"applyAfterDraw\":%u,\"sequence\":%llu,\"native\":%s,\"result\":%d,\"viewBits\":[",
    frameId,drawId+1,pending.scene,pending.camera,pending.afterDraw,pending.sequence,native?"true":"false",result);
  uint32_t words[32]{};memcpy(words,pending.info.view,128);
  for(unsigned i=0;i<32;++i){if(i==16)fputs("],\"projectionBits\":[",output);fprintf(output,"%s%u",i%16?",":"",words[i]);}
  fputs("]}\n",output);fflush(output);
}
static void Missed(const char* reason) {
  attemptedFrame=frameId;++missedFirstDraws;
  if(CanLog()&&(frameId<10||frameId%300==0||frameId+120==traceUntilFrame)) {
    fprintf(output,"{\"event\":\"first_window_missed\",\"frame\":%u,\"beforeDraw\":%u,\"reason\":\"%s\"}\n",frameId,drawId+1,reason);fflush(output);
  }
}
static void AtDraw(IDirect3DDevice9* device,uint32_t scene,uint32_t main) {
  if(!enabled||attemptedFrame==frameId)return;
  D3DMATRIX view{},projection{};IDirect3DSurface9* target=nullptr;
  if(FAILED(device->GetTransform(D3DTS_VIEW,&view))||FAILED(device->GetTransform(D3DTS_PROJECTION,&projection))) {Missed("unreadable_transforms");return;}
  if(projection._34==0||projection._44!=0)return;
  // Conservatively close the window even for a foreign/offscreen perspective
  // draw: the renderer might classify it as Main. Never report a late API
  // SUCCESS as proof that first-update-wins accepted the camera this frame.
  if(!pending.valid||pending.frame!=frameId||pending.deviceEpoch!=deviceEpoch||pending.scene!=scene||pending.camera!=main) {Missed("missing_current_native_apply");return;}
  if(memcmp(&view,pending.info.view,64)||memcmp(&projection,pending.info.projection,64)) {++stateMismatches;Missed("different_camera");return;}
  if(FAILED(device->GetRenderTarget(0,&target))||!target){Missed("unreadable_target");return;}
  const auto found=primaryTargets.find(device);const bool primary=found!=primaryTargets.end()&&found->second==target;
  target->Release();if(!primary){Missed("non_primary_target");return;}
  if(observedFrame!=frameId){observedFrame=frameId;selected=pending;++matched;}
  if(!submitEnabled) {
    attemptedFrame=frameId;
    if(frameId%300==0||frameId+120==traceUntilFrame)LogPacket(REMIXAPI_ERROR_CODE_SUCCESS,false);
    return;
  }
  attemptedFrame=frameId;auto api=GetRemixApi();
  const auto result=api&&api->SetupCamera?api->SetupCamera(&pending.info):REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
  if(result==REMIXAPI_ERROR_CODE_SUCCESS){submittedFrame=frameId;++submitted;}
  else ++failures;
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS||frameId%300==0||frameId+120==traceUntilFrame)LogPacket(result,true);
}
#if defined(_M_IX86)
static DWORD ownerThread;
using NativeApply=unsigned char(__thiscall*)(void*);
static NativeApply originalApply;
static unsigned char __fastcall Apply(void* camera,void*) {
  const auto result=originalApply(camera);
  if(!enabled||GetCurrentThreadId()!=ownerThread)return result;
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(!result){if(pending.camera==uint32_t(reinterpret_cast<uintptr_t>(camera)))pending.valid=false;++rejected;return result;}
  const auto engine=scene_geometry::Word(0x755274),scene=engine?scene_geometry::Word(engine+0x18):0;
  const auto main=engine?scene_geometry::Word(engine+0x1c):0,address=uint32_t(reinterpret_cast<uintptr_t>(camera));
  if(!main||main!=address){++foreignApplies;return result;}
  abi::spCameraObservedLayout raw{};
  if(!scene_geometry::Read(address,&raw,sizeof(raw))){pending.valid=false;++rejected;return result;}
  Capture(scene,address,main,scene_geometry::Word(address),raw);return result;
}
static bool Install() {
  // push esi; mov esi,ecx; test byte ptr [esi+224],1. Three complete
  // position-independent instructions; preserve flags for the following JZ.
  constexpr uint32_t address=0x427d40;
  constexpr unsigned char expected[10]={0x56,0x8b,0xf1,0xf6,0x86,0x24,2,0,0,1};
  unsigned char actual[10]{};
  if(!scene_geometry::Read(address,actual,10)||memcmp(actual,expected,10))return false;
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,15,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
  if(!trampoline)return false;
  memcpy(trampoline,expected,10);trampoline[10]=0xe9;
  uint32_t jump=address+10-uint32_t(reinterpret_cast<uintptr_t>(trampoline)+15);memcpy(trampoline+11,&jump,4);
  DWORD previous=0,ignored=0;
  if(!VirtualProtect(trampoline,15,PAGE_EXECUTE_READ,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  auto entry=reinterpret_cast<void*>(address);
  if(!VirtualProtect(entry,10,PAGE_EXECUTE_READWRITE,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  originalApply=reinterpret_cast<NativeApply>(trampoline);ownerThread=GetCurrentThreadId();
  unsigned char replacement[10]={0xe9,0,0,0,0,0x90,0x90,0x90,0x90,0x90};
  jump=uint32_t(reinterpret_cast<uintptr_t>(&Apply))-address-5;memcpy(replacement+1,&jump,4);
  memcpy(entry,replacement,10);VirtualProtect(entry,10,previous,&ignored);
  FlushInstructionCache(GetCurrentProcess(),trampoline,15);FlushInstructionCache(GetCurrentProcess(),entry,10);return true;
}
#endif
static void Prepare(IDirect3DDevice9* device) {
#if defined(_M_IX86)
  if(!enabled||GetCurrentThreadId()!=ownerThread)return;
  const auto engine=scene_geometry::Word(0x755274);
  AtDraw(device,engine?scene_geometry::Word(engine+0x18):0,engine?scene_geometry::Word(engine+0x1c):0);
#else
  (void)device;
#endif
}
static void Reset() {++deviceEpoch;pending={};selected={};observedFrame=submittedFrame=attemptedFrame=UINT32_MAX;}
static void Initialize() {
  wchar_t path[MAX_PATH]{},option[8]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_CAMERA_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;
#if defined(_M_IX86)
  if(!scene_audit::VerifiedImage())return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);enabled=Install();
  submitEnabled=enabled&&GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_CAMERA_SUBMIT",option,8)&&wcscmp(option,L"1")==0;
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"submit\":%s,\"maxLogBytes\":16777216,\"source\":\"native_camera_after_apply\",\"transport\":\"before_first_matching_primary_draw\"}\n",enabled?"true":"false",submitEnabled?"true":"false");fflush(output);}
#else
  (void)option;
#endif
}
static void EndFrame() {
  if(CanLog()&&(applies||foreignApplies||rejected||matched||failures||stateMismatches||lateUpdates||missedFirstDraws||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"deviceEpoch\":%llu,\"applies\":%u,\"foreignApplies\":%u,\"rejected\":%u,\"matched\":%u,\"submitted\":%u,\"failures\":%u,\"stateMismatches\":%u,\"lateUpdates\":%u,\"missedFirstDraws\":%u}\n",
      frameId,deviceEpoch,applies,foreignApplies,rejected,matched,submitted,failures,stateMismatches,lateUpdates,missedFirstDraws);fflush(output);
  }
  applies=foreignApplies=rejected=matched=submitted=failures=stateMismatches=lateUpdates=missedFirstDraws=0;
}
} // namespace native_camera_source
