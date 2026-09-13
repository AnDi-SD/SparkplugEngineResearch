// Own phase observer. Original native world updates run once, unchanged.
// Tokens prove a completed plain System Root dispatch, not arbitrary descendant
// update coverage or persistent object lifetime. See world-update-witness doc.
#pragma once
#include "../../Sparkplug/Analysis/PC/SparkplugAbi.h"
#include <intrin.h>
namespace native_update_source {
namespace abi=sparkplug::evidence::pc;
struct Witness {
  uint32_t manager,scene,systemRoot,system,root,frame;
  uint64_t sequence,mutation,deviceEpoch;
  DWORD thread;
  bool valid;
};
static bool enabled;
static FILE* output;
static unsigned begun,completed,roots,recorded,published,aborted,overflowed,queries,accepted;
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
#if defined(_M_IX86)
static DWORD ownerThread;
static bool attempted;
constexpr unsigned capacity=64;
__declspec(align(8)) static volatile LONG64 invalidationSerial=0;
static volatile LONG activeUpdates=0;
static thread_local unsigned rootDepth;
#if defined(WINX_REMIX_TEST)
// Only owned fixture addresses replace platform inputs. Never map native VAs.
static uintptr_t managerPointerAddress=0x75db90,managerEntry=0x45a7d0;
static uintptr_t rootSlotAddress=0x6dc524,rootOriginalEntry=0x421420,rootReturnAddress=0x45a7f2;
#else
static constexpr uintptr_t managerPointerAddress=0x75db90,managerEntry=0x45a7d0;
static constexpr uintptr_t rootSlotAddress=abi::spNodeVTable+abi::spNodeWorldUpdateVirtualByteOffset;
static constexpr uintptr_t rootOriginalEntry=abi::spNodeWorldUpdateProtectedEntry,rootReturnAddress=0x45a7f2;
#endif
using NativeManager=uint32_t(__thiscall*)(void*);
using NativeRoot=uint32_t(__thiscall*)(void*,uint32_t);
static NativeManager originalManager;
static NativeRoot originalRoot;
static unsigned char managerReplacement[7];
static Witness tokens[capacity];
static unsigned tokenCount;
struct Batch {
  Batch* parent;
  uint32_t manager,frame;
  uint64_t sequence,mutation,deviceEpoch;
  unsigned count;
  bool eligible,invalid;
  Witness pending[capacity];
};
static thread_local Batch* active;
static uint64_t Serial(){return static_cast<uint64_t>(InterlockedCompareExchange64(&invalidationSerial,0,0));}
static uint64_t Invalidate(){return static_cast<uint64_t>(InterlockedIncrement64(&invalidationSerial));}
static LONG Running(){return InterlockedCompareExchange(&activeUpdates,0,0);}
template<class T> static bool Read(uintptr_t address,T& value){return scene_geometry::Read(address,&value,sizeof(value));}
static uint32_t Pointer(const void* p){return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));}
static uint32_t __fastcall ManagerUpdate(void*,void*);
static uint32_t __fastcall RootUpdate(void*,void*,uint32_t);
static bool InstalledIdentity() {
  uint32_t slot=0;unsigned char code[7]{};
  return Read(rootSlotAddress,slot)&&slot==Pointer(reinterpret_cast<void*>(&RootUpdate))&&
    scene_geometry::Read(managerEntry,code,sizeof(code))&&!memcmp(code,managerReplacement,sizeof(code));
}
static bool IsPlainWorldSlot(uint32_t functionAddress) {
  return functionAddress==rootOriginalEntry||(enabled&&
    functionAddress==Pointer(reinterpret_cast<void*>(&RootUpdate))&&InstalledIdentity());
}
static bool ManagerIdentity(uint32_t manager,bool idle) {
  uint32_t singleton=0;abi::spSceneManagerLayout value{};
  return Read(managerPointerAddress,singleton)&&singleton==manager&&manager&&Read(manager,value)&&
    scene_geometry::Word(manager)==0x6e7154&&(!idle||value.currentScene==0);
}
// Pure borrowed-state qualification: no accounting, game writes or callbacks.
static bool SceneIdentity(const Witness& w,bool insideRoot) {
  abi::spSceneLayout scene{};abi::spSceneManagerLayout manager{};
  uint32_t primary=0,nodePrimary=0,nodeScene=0,parent=0,system=0,partitionRoot=0;
  if(!ManagerIdentity(w.manager,false)||!Read(w.manager,manager)||!Read(w.scene,scene)||
     !Read(w.scene,primary)||primary!=0x6e7358||scene.systemRoot!=w.systemRoot||!w.systemRoot||
     !Read(w.systemRoot,nodePrimary)||nodePrimary!=abi::spNodeVTable||
     !Read(w.systemRoot+0x3c,nodeScene)||nodeScene!=w.scene||!Read(w.systemRoot+0x2c,parent)||parent||
     !Read(w.scene+0x38,system)||system!=w.system)return false;
  if(system&&!Read(system+0x1d4,partitionRoot))return false;
  return partitionRoot==w.root&&manager.currentScene==(insideRoot?w.scene:0);
}
static bool BatchCurrent(const Batch& b) {
  return b.eligible&&!b.invalid&&active==&b&&!b.parent&&GetCurrentThreadId()==ownerThread&&
    Running()==1&&frameId==b.frame&&native_camera_source::deviceEpoch==b.deviceEpoch&&
    scene_geometry::MutationSerial()==b.mutation&&Serial()==b.sequence;
}
static bool ReadRootState(const Batch& b,uint32_t root,Witness& out) {
  out={};abi::spSceneManagerLayout manager{};abi::spSceneLayout scene{};
  if(!Read(b.manager,manager)||!manager.currentScene||!Read(manager.currentScene,scene))return false;
  Witness w{};w.manager=b.manager;w.scene=manager.currentScene;w.systemRoot=root;
  w.system=scene.partitionSystem;if(w.system&&!Read(w.system+0x1d4,w.root))return false;
  w.frame=b.frame;w.sequence=b.sequence;w.mutation=b.mutation;w.deviceEpoch=b.deviceEpoch;w.thread=ownerThread;
  if(!SceneIdentity(w,true)||!InstalledIdentity()||!BatchCurrent(b))return false;
  w.valid=true;out=w;return true;
}
static bool QualifyWitness(const Witness& w,uint32_t scene,uint32_t frame,uint64_t mutation) {
  if(!enabled||!w.valid||GetCurrentThreadId()!=ownerThread||w.thread!=ownerThread||active||Running()!=0||
     scene!=w.scene||frame!=w.frame||frameId!=frame||mutation!=w.mutation||
     native_camera_source::deviceEpoch!=w.deviceEpoch||scene_geometry::MutationSerial()!=mutation||Serial()!=w.sequence)return false;
  // Last atomic fences follow every borrowed read, including installed bytes.
  return SceneIdentity(w,false)&&InstalledIdentity()&&scene_geometry::MutationSerial()==mutation&&
    frameId==frame&&native_camera_source::deviceEpoch==w.deviceEpoch&&Serial()==w.sequence&&Running()==0;
}
static bool GetWitness(uint32_t scene,uint32_t frame,uint64_t mutation,Witness& out) {
  out={};if(!enabled||GetCurrentThreadId()!=ownerThread)return false;++queries;
  for(unsigned i=0;i<tokenCount;++i)if(tokens[i].scene==scene&&QualifyWitness(tokens[i],scene,frame,mutation)){
    out=tokens[i];++accepted;return true;
  }
  return false;
}
static void Publish(Batch* b) {
  if(!BatchCurrent(*b)||!ManagerIdentity(b->manager,true)||!InstalledIdentity())return;
  // Revalidate all pending identities at completion; partial stale publication
  // is unnecessary. Equal pointers do not claim absence of untracked ABA.
  for(unsigned i=0;i<b->count;++i)if(!SceneIdentity(b->pending[i],false)){b->invalid=true;return;}
  if(!BatchCurrent(*b))return;
  memcpy(tokens,b->pending,b->count*sizeof(Witness));tokenCount=b->count;
  ++completed;published+=b->count;
}
static uint32_t ManagerAndRestore(Batch* b,void* object) {
  uint32_t result=0;bool normal=false;
  __try {result=originalManager(object);normal=true;Publish(b);}
  __finally {
    if(GetCurrentThreadId()==ownerThread&&(!normal||b->invalid))++aborted;
    active=b->parent;InterlockedDecrement(&activeUpdates);
  }
  return result;
}
static uint32_t __fastcall ManagerUpdate(void* object,void*) {
  if(!enabled)return originalManager(object);
  Batch batch{};batch.parent=active;
  // Activity precedes invalidation: a preempted foreign entrant must not let
  // an owner publish a newer token before that foreign producer starts work.
  const auto running=InterlockedIncrement(&activeUpdates);
  batch.sequence=Invalidate();
  const bool owner=GetCurrentThreadId()==ownerThread;
  if(owner){tokenCount=0;++begun;batch.manager=Pointer(object);batch.frame=frameId;
    batch.mutation=scene_geometry::MutationSerial();batch.deviceEpoch=native_camera_source::deviceEpoch;}
  // Foreign-thread batches never read render-thread metadata or native objects.
  batch.eligible=owner&&!batch.parent&&running==1&&ManagerIdentity(batch.manager,true)&&InstalledIdentity();
  if(batch.parent)batch.parent->invalid=true;
  active=&batch;return ManagerAndRestore(&batch,object);
}
static uint32_t RootAndRestore(Batch* b,Witness* w,void* object,uint32_t flags,bool standalone) {
  uint32_t result=0;bool normal=false;
  __try {
    result=originalRoot(object,flags);normal=true;
    if(w->valid&&b&&BatchCurrent(*b)&&SceneIdentity(*w,true)&&InstalledIdentity()&&BatchCurrent(*b)) {
      if(b->count==capacity){b->invalid=true;++overflowed;}
      else {b->pending[b->count++]=*w;++recorded;}
    }
  } __finally {
    if(!normal&&b)b->invalid=true;
    --rootDepth;
    if(standalone)InterlockedDecrement(&activeUpdates);
  }
  return result;
}
static uint32_t RootWithCaller(void* object,uint32_t flags,uintptr_t caller) {
  if(!enabled)return originalRoot(object,flags);
  auto* b=active;const bool owner=GetCurrentThreadId()==ownerThread;
  // A world producer outside a manager batch also invalidates previous phase
  // evidence. Descendant calls within a batch stay on the unchanged native path.
  const bool standalone=!b;
  if(standalone){InterlockedIncrement(&activeUpdates);Invalidate();}
  Witness w{};if(owner)++roots;
  if(owner&&b&&rootDepth==0&&caller==rootReturnAddress&&flags==0&&BatchCurrent(*b))
    ReadRootState(*b,Pointer(object),w);
  ++rootDepth;return RootAndRestore(b,&w,object,flags,standalone);
}
static uint32_t __fastcall RootUpdate(void* object,void*,uint32_t flags) {
  const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
  return RootWithCaller(object,flags,caller);
}
static bool Install() {
  constexpr unsigned char prefix[7]={0x56,0x57,0x8b,0xf9,0x8b,0x47,0x18};
  unsigned char observed[7]{};uint32_t slot=0;
  if((rootSlotAddress&3)||!Read(rootSlotAddress,slot)||slot!=rootOriginalEntry||
     !scene_geometry::Read(managerEntry,observed,7)||memcmp(observed,prefix,7))return false;
  auto* trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,12,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  if(!trampoline)return false;
  memcpy(trampoline,prefix,7);trampoline[7]=0xe9;
  const auto back=uint32_t(managerEntry+7-reinterpret_cast<uintptr_t>(trampoline+12));memcpy(trampoline+8,&back,4);
  DWORD old=0;
  if(!VirtualProtect(trampoline,12,PAGE_EXECUTE_READ,&old)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  FlushInstructionCache(GetCurrentProcess(),trampoline,12);
  DWORD protections[2]{};const uintptr_t addresses[2]={rootSlotAddress,managerEntry};const unsigned sizes[2]={4,7};
  unsigned writable=0;
  for(;writable<2;++writable)if(!VirtualProtect(reinterpret_cast<void*>(addresses[writable]),sizes[writable],PAGE_EXECUTE_READWRITE,&protections[writable]))break;
  bool ready=writable==2;
  if(ready)ready=Read(rootSlotAddress,slot)&&slot==rootOriginalEntry&&scene_geometry::Read(managerEntry,observed,7)&&!memcmp(observed,prefix,7);
  if(!ready){while(writable){--writable;VirtualProtect(reinterpret_cast<void*>(addresses[writable]),sizes[writable],protections[writable],&old);}
    VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  memset(managerReplacement,0x90,7);managerReplacement[0]=0xe9;
  const auto jump=uint32_t(reinterpret_cast<uintptr_t>(&ManagerUpdate)-managerEntry-5);memcpy(managerReplacement+1,&jump,4);
  originalManager=reinterpret_cast<NativeManager>(trampoline);originalRoot=reinterpret_cast<NativeRoot>(rootOriginalEntry);
  // Startup transaction: both locations were verified/writable before either
  // write. Original pointers are published first; Node alone safely forwards.
  // The seven-byte entry patch requires the existing startup quiescence rule;
  // it is not a claim of atomic modification against a concurrent instruction fetch.
  InterlockedExchangePointer(reinterpret_cast<void* volatile*>(rootSlotAddress),reinterpret_cast<void*>(&RootUpdate));
  memcpy(reinterpret_cast<void*>(managerEntry),managerReplacement,7);
  for(unsigned i=2;i;--i){const auto n=i-1;VirtualProtect(reinterpret_cast<void*>(addresses[n]),sizes[n],protections[n],&old);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(addresses[n]),sizes[n]);}
  return true;
}
#else
static bool IsPlainWorldSlot(uint32_t){return false;}
static bool QualifyWitness(const Witness&,uint32_t,uint32_t,uint64_t){return false;}
static bool GetWitness(uint32_t,uint32_t,uint64_t,Witness& out){out={};return false;}
#endif
static void Initialize() {
#if defined(_M_IX86)
  if(attempted)return;
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_UPDATE_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;attempted=true;output=_wfsopen(path,L"wb",_SH_DENYNO);
  // Call before SceneAudit installs its entry hook. Do not weaken full startup
  // VerifiedImage into just the two local hook signatures.
  const bool verified=scene_audit::VerifiedImage();
  if(verified){ownerThread=GetCurrentThreadId();enabled=Install();}
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"imageVerified\":%s,\"capacity\":64,\"scope\":\"plain System Root completion only; borrowed phase witness\"}\n",enabled?"true":"false",verified?"true":"false");fflush(output);}
#endif
}
static void EndFrame() {
#if defined(_M_IX86)
  if(!enabled)return;Invalidate();if(GetCurrentThreadId()!=ownerThread)return;
  tokenCount=0;
  if(CanLog()&&(begun||roots||queries||frameId%300==0)){
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"sequence\":%llu,\"begun\":%u,\"completed\":%u,\"rootCalls\":%u,\"recorded\":%u,\"published\":%u,\"aborted\":%u,\"overflowed\":%u,\"queries\":%u,\"accepted\":%u}\n",frameId,Serial(),begun,completed,roots,recorded,published,aborted,overflowed,queries,accepted);fflush(output);}
  begun=completed=roots=recorded=published=aborted=overflowed=queries=accepted=0;
#endif
}
}
