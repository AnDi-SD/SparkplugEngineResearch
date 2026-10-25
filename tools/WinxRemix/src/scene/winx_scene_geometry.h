// Own RTX render-pass extension. Enumerates existing native render ownership;
// the original engine still submits transforms, materials, shaders and meshes.
// No visibility algorithm or reconstructed game logic is replaced.
#include "winx_scene_geometry_snapshot.h"

#if defined(_M_IX86)
namespace scene_geometry {
using scene_audit::At;
constexpr unsigned limit=winx_remix::SceneGeometrySnapshotSource::kLimit;
// This adapter executes on the game's render thread. ReadProcessMemory for
// every small record needlessly enters the kernel thousands of times/frame.
// Keep the same address/size caps and reject inaccessible native records.
static bool Read(uintptr_t address,void* data,size_t size) {
  if(address<0x10000 || address>=0x7fff0000 || size>16384 || size>0x7fff0000-address) return false;
  __try {memcpy(data,reinterpret_cast<const void*>(address),size);}
  __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION || GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR?
      EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return false;}
  return true;
}
static uint32_t Word(uintptr_t address) {uint32_t value=0;Read(address,&value,4);return value;}
// Adapter metadata. Retirement can arrive on another thread, so 64-bit reads
// must also be atomic on x86. This does not identify native object lifetimes.
__declspec(align(8)) static volatile LONG64 registryMutationSerial=1;
static uint64_t MutationSerial() {
  return static_cast<uint64_t>(InterlockedCompareExchange64(&registryMutationSerial,0,0));
}
static uint64_t AdvanceMutationSerial() {
  return static_cast<uint64_t>(InterlockedIncrement64(&registryMutationSerial));
}
using Registry=winx_remix::SceneGeometrySnapshot;
using OccurrenceSource=Registry::OccurrenceSource;
using SupportOccurrence=Registry::SupportOccurrence;
constexpr unsigned occurrenceLimit=winx_remix::SceneGeometrySnapshotSource::kOccurrenceLimit;

static bool Vector(uintptr_t object,unsigned offset,std::vector<uint32_t>& result) {
  return winx_remix::SceneGeometrySnapshotSource::ReadVector(uint32_t(object),offset,result,
    [](uint32_t address,void* data,size_t size){return Read(address,data,size);});
}
static bool Support(uint32_t object,uint32_t scene,bool partition,uint32_t& adjusted) {
  return winx_remix::SceneGeometrySnapshotSource::ReadSupport(object,scene,partition,adjusted,
    [](uint32_t address,void* data,size_t size){return Read(address,data,size);});
}
static Registry ReadRegistry(uint32_t scene) {
  return winx_remix::SceneGeometrySnapshotSource{}.Capture(scene,
    [](uint32_t address,void* data,size_t size){return Read(address,data,size);},
    []{return MutationSerial();});
}

static void Event(const char* event,uint32_t scene,unsigned original,unsigned added,unsigned nodes=0,unsigned elapsedMs=0) {
  if(sceneGeometryLog && _ftelli64(sceneGeometryLog)<16*1024*1024) {
    fprintf(sceneGeometryLog,"{\"event\":\"%s\",\"frame\":%u,\"draw\":%u,\"scene\":%u,\"original\":%u,\"added\":%u,\"nodes\":%u,\"elapsedMs\":%u}\n",event,frameId,drawId,scene,original,added,nodes,elapsedMs);
    fflush(sceneGeometryLog);
  }
}
struct Scope;
static thread_local Scope* active;
// Separate passive consumer of the original selection. It runs even when the
// existing visibility extension/independent observer is disabled or ObserveOnly.
static void (*observeOriginalSelection)(Scope&,uint32_t,uint32_t,uint32_t) noexcept;
// Optional own before-prepare reader. It receives the original selection and
// the current graph before this adapter extends visibility.
static void (*observeSelection)(Scope&,const Registry&,uint32_t);
static bool (*skipExtendedSupport)(uint32_t);
static void (*selectionRestarted)(uint64_t);
__declspec(align(8)) static volatile LONG64 nextScopeSerial=0;
#if defined(WINX_REMIX_TEST)
static uintptr_t enginePointerAddress=0x755274;
#else
static constexpr uintptr_t enginePointerAddress=0x755274;
#endif
struct Scope {
  // Unique adapter operation identity, not a durable native object generation.
  const uint64_t serial=static_cast<uint64_t>(InterlockedIncrement64(&nextScopeSerial));
  Scope* parent=active;
  uint32_t scene,camera,manager=0,original[3]{},replacement[3]{};
  unsigned stamp=0;bool attempted=false;
  ULONGLONG started=0;
  std::vector<uint32_t> expanded;
  std::vector<std::pair<uint32_t,uint32_t>> marks;
  Scope(uint32_t s,uint32_t c):scene(s),camera(c) {
    if(parent){if(selectionRestarted)selectionRestarted(parent->serial);parent->Restore();}active=this;
  }
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
  ~Scope() {Restore();active=parent;}
  // The returned reference and all addresses in it expire with this scope.
  // Failed reads are also cached; mutation does not silently refresh a snapshot
  // already consumed by an owner join. Consumers must check mutationSerial and
  // the current native scene/system/root before attaching borrowed provenance.
  const Registry& RegistrySnapshot() {
    if(!registryRead) {
      registryRead=true;
      try {registrySnapshot=ReadRegistry(scene);}
      catch(const std::bad_alloc&) {Event("allocation_rejected",scene,0,0);}
    }
    return registrySnapshot;
  }
  void Restore() {
    if(!manager) return;
    uint32_t now[3]{};
    if(Read(manager+0x2c,now,sizeof(now)) && memcmp(now,replacement,sizeof(now))==0) {
      memcpy(reinterpret_cast<void*>(manager+0x2c),original,sizeof(original));
      if(frameId%300==0 || triggered) Event("restored",scene,(original[1]-original[0])/4,static_cast<unsigned>(marks.size()));
    } else {sceneGeometryEnabled=false;Event("ownership_conflict",scene,0,0);}
    for(const auto& mark:marks) if(Word(mark.first+0x64)==stamp)
      memcpy(reinterpret_cast<void*>(mark.first+0x64),&mark.second,4);
    manager=0;marks.clear();
  }
  void Extend(uint32_t selectedManager,uint32_t selectedScene,uint32_t selectedCamera) {
    if(attempted || parent || !sceneGeometryEnabled || sceneGeometryObserveOnly || keepSceneGeometryForComparison ||
       scene!=selectedScene || camera!=selectedCamera) return;
    attempted=true;
    const auto engine=Word(enginePointerAddress);
    if(!engine || Word(engine+0x18)!=scene || Word(engine+0x1c)!=camera ||
       Word(selectedManager)!=0x6e8cdc || Word(selectedManager+0x38)) return;
    unsigned char cameraRaw[0x238]{};
    if(!Read(camera,cameraRaw,sizeof(cameraRaw)) || cameraRaw[0x231] || At(cameraRaw,0x138)!=0x3f800000 || At(cameraRaw,0x148)!=0) return;
    started=GetTickCount64();const auto& registry=RegistrySnapshot();
    if(!registry.valid || registry.mutationSerial!=MutationSerial()) {if(frameId%300==0)Event("registry_rejected",scene,0,0);return;}
    if(observeSelection) {
      try {observeSelection(*this,registry,selectedManager);}
      catch(const std::bad_alloc&) {Event("observer_allocation_rejected",scene,0,0);}
    }
    ExtendSelection(selectedManager,registry);
  }
  void ExtendSelection(uint32_t selectedManager,const Registry& registry) {
    if(!registry.valid || registry.mutationSerial!=MutationSerial())return;
    if(!Vector(selectedManager,0x28,expanded)) {Event("selection_rejected",scene,0,0);return;}
    const unsigned before=static_cast<unsigned>(expanded.size());
    std::set<uint32_t> present(expanded.begin(),expanded.end());
    unsigned independentlySubmitted=0;
    for(auto support:registry.supports) if(present.insert(support).second) {
      if(skipExtendedSupport&&skipExtendedSupport(support)){++independentlySubmitted;continue;}
      if(expanded.size()>=limit) {Event("selection_limit",scene,before,0,registry.nodes);return;}
      expanded.push_back(support);marks.emplace_back(support,Word(support+0x64));
    }
    if(independentlySubmitted&&(frameId%300==0||triggered))Event("independent_omitted",scene,before,independentlySubmitted,registry.nodes);
    if(marks.empty()) return;
    if(!Read(selectedManager+0x2c,original,sizeof(original))) return;
    // Borrow this scope's stable storage only until original SceneRender returns.
    // Never pass our allocation to a native clear/reallocate/destructor.
    replacement[0]=reinterpret_cast<uintptr_t>(expanded.data());
    replacement[1]=replacement[0]+static_cast<uint32_t>(expanded.size()*4);
    replacement[2]=replacement[1];stamp=Word(selectedManager+0x14);manager=selectedManager;
    for(const auto& mark:marks) memcpy(reinterpret_cast<void*>(mark.first+0x64),&stamp,4);
    memcpy(reinterpret_cast<void*>(manager+0x2c),replacement,sizeof(replacement));
    if(frameId%300==0 || frameId<2 || triggered) Event("expanded",scene,before,static_cast<unsigned>(marks.size()),registry.nodes,
      started?static_cast<unsigned>(GetTickCount64()-started):0);
    if(sceneGeometryLog && _ftelli64(sceneGeometryLog)<16*1024*1024 && (frameId%300==0 || triggered))
      fprintf(sceneGeometryLog,"{\"event\":\"registry\",\"frame\":%u,\"scene\":%u,\"supported\":%u,\"dynamic\":%u,\"unsupportedDynamic\":%u}\n",
        frameId,scene,static_cast<unsigned>(registry.supports.size()),registry.dynamic,registry.unsupportedDynamic);
  }
private:
  bool registryRead=false;
  Registry registrySnapshot;
};
static void BeforeSelect() {
  if(active){if(active->attempted&&selectionRestarted)selectionRestarted(active->serial);active->Restore();}
}
static void AfterSelect(uint32_t manager,uint32_t scene,uint32_t camera) {
  if(!active) return;
  if(observeOriginalSelection)observeOriginalSelection(*active,manager,scene,camera);
  try {active->Extend(manager,scene,camera);}
  catch(const std::bad_alloc&) {active->Restore();Event("allocation_rejected",scene,0,0);}
}
using NativeRender=uintptr_t(__thiscall*)(void*,void*);
static NativeRender originalRender;
static DWORD renderThread;
static uintptr_t RenderAndRestore(Scope* scope,void* scene,void* camera) {
  uintptr_t result=0;
  __try {result=originalRender(scene,camera);}
  __finally {scope->Restore();active=scope->parent;}
  return result;
}
static uintptr_t __fastcall Render(void* scene,void*,void* camera) {
  if((!sceneGeometryEnabled && !sceneGeometryObservationEnabled) || GetCurrentThreadId()!=renderThread) return originalRender(scene,camera);
  Scope scope(reinterpret_cast<uintptr_t>(scene),reinterpret_cast<uintptr_t>(camera));
  return RenderAndRestore(&scope,scene,camera);
}
static bool Install() {
  // Five bytes, three complete position-independent instructions: sub esp,20; push ebx; push ebp.
  constexpr unsigned char expected[5]={0x83,0xec,0x20,0x53,0x55};
  auto entry=reinterpret_cast<unsigned char*>(0x45ec70);unsigned char actual[5]{};
  if(!Read(0x45ec70,actual,5) || memcmp(actual,expected,5)) return false;
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,10,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
  if(!trampoline) return false;
  memcpy(trampoline,expected,5);trampoline[5]=0xe9;
  uint32_t jump=0x45ec75-uint32_t(reinterpret_cast<uintptr_t>(trampoline)+10);memcpy(trampoline+6,&jump,4);
  DWORD previous=0,ignored=0;
  if(!VirtualProtect(trampoline,10,PAGE_EXECUTE_READ,&previous)) {VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  FlushInstructionCache(GetCurrentProcess(),trampoline,10);
  if(!VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&previous)) {VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  originalRender=reinterpret_cast<NativeRender>(trampoline);renderThread=GetCurrentThreadId();
  unsigned char replacement[5]={0xe9};jump=uint32_t(reinterpret_cast<uintptr_t>(&Render)-0x45ec75);memcpy(replacement+1,&jump,4);
  memcpy(entry,replacement,5);VirtualProtect(entry,5,previous,&ignored);FlushInstructionCache(GetCurrentProcess(),entry,5);return true;
}
} // namespace scene_geometry
#endif
