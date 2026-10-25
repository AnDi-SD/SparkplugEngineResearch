#pragma once
// Own passive PC entry observer. The original DXLight world producer executes
// once, with its full argument and opaque EAX. No controller/world replay or
// dirty-state repair. Correlation publishes only after Root AND Manager return.
#include "../lighting/winx_light_world_observer.h"
#include <memory>
#include <new>

namespace native_light_world_source {
static bool enabled;
static FILE* output;
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
#if defined(_M_IX86)
namespace update=native_update_source;
using Observer=winx_remix::LightWorldObserver;
using Record=winx_remix::SceneLightSnapshot::Record;
using NativeLight=uint32_t(__thiscall*)(void*,uint32_t);
static std::unique_ptr<Observer> observer;
static Observer::Context collectingContext{};
static DWORD ownerThread;
static bool attempted,installed;
static NativeLight original;
__declspec(align(8)) static volatile LONG64 invalidationEpoch=1;
static volatile LONG activeLights,calls,correlated,rejected,batches,sealed;
static unsigned char replacement[8];
#if defined(WINX_REMIX_TEST)
static uintptr_t lightEntry=0x4b58d0,lightSlotAddress=0x6f0cb8;
#else
static constexpr uintptr_t lightEntry=0x4b58d0,lightSlotAddress=0x6f0cb8;
#endif
static uint64_t Epoch(){return static_cast<uint64_t>(InterlockedCompareExchange64(&invalidationEpoch,0,0));}
static uint64_t InvalidateEpoch(){return static_cast<uint64_t>(InterlockedIncrement64(&invalidationEpoch));}
static LONG Running(){return InterlockedCompareExchange(&activeLights,0,0);}
static uint32_t At(const unsigned char* bytes,unsigned offset){uint32_t value;memcpy(&value,bytes+offset,4);return value;}
static bool ReadRecord(void* object,Record& record){
  record.address=update::Pointer(object);
  return scene_geometry::Read(record.address,record.raw,sizeof(record.raw));
}
static Observer::Context Context(const update::Batch& batch){
  return {batch.manager,batch.frame,ownerThread,batch.sequence,batch.mutation,batch.deviceEpoch,Epoch()};
}
static bool SameEpochs(){
  return GetCurrentThreadId()==ownerThread&&frameId==collectingContext.frame&&
    native_camera_source::deviceEpoch==collectingContext.deviceEpoch&&scene_geometry::MutationSerial()==collectingContext.mutation&&
    update::Serial()==collectingContext.sequence&&Epoch()==collectingContext.observerEpoch;
}
static bool InstalledIdentity(){
  unsigned char code[8]{};uint32_t slot=0;
  return installed&&update::enabled&&update::InstalledIdentity()&&update::Read(lightSlotAddress,slot)&&
    slot==lightEntry&&scene_geometry::Read(lightEntry,code,sizeof(code))&&!memcmp(code,replacement,sizeof(code));
}
static void BatchBegan(const update::Batch& batch) noexcept {
  InvalidateEpoch();
  if(GetCurrentThreadId()!=ownerThread)return;
  if(!enabled||!observer)return;
  InterlockedIncrement(&batches);
  collectingContext=Context(batch);
  if(!batch.eligible||Running()!=0||!update::BatchCurrent(batch)||!InstalledIdentity())observer->Invalidate();
  else observer->Begin(collectingContext);
}
static bool RootPublished(uint32_t scene,uint32_t root,const update::Batch& batch){
  for(unsigned i=0;i<update::tokenCount;++i){const auto& token=update::tokens[i];
    if(token.scene==scene&&token.systemRoot==root&&token.sequence==batch.sequence&&token.frame==batch.frame&&
       token.mutation==batch.mutation&&token.deviceEpoch==batch.deviceEpoch&&token.thread==ownerThread&&
       update::SceneIdentity(token,false))return true;
  }
  return false;
}
static void BatchFinished(const update::Batch& batch,bool completed) noexcept {
  if(GetCurrentThreadId()!=ownerThread){InvalidateEpoch();return;}
  if(!enabled||!observer)return;
  bool accepted=false;
  if(completed&&Running()==0&&Context(batch)==collectingContext&&update::BatchCurrent(batch)&&InstalledIdentity())
    accepted=observer->Seal(collectingContext,[&](uint32_t scene,uint32_t root) noexcept {
      return RootPublished(scene,root,batch)&&SameEpochs()&&Running()==0&&update::BatchCurrent(batch);
    });
  if(accepted&&(!SameEpochs()||Running()!=0||!update::BatchCurrent(batch)))accepted=false;
  if(!accepted)observer->Invalidate();else InterlockedIncrement(&sealed);
  if(CanLog())fprintf(output,"{\"event\":\"batch\",\"frame\":%u,\"manager\":%u,\"sequence\":%llu,\"observerEpoch\":%llu,\"published\":%s,\"observed\":%u}\n",
    batch.frame,batch.manager,batch.sequence,Epoch(),accepted?"true":"false",static_cast<unsigned>(observer->Count()));
}
static bool CallContextCurrent(){
  const auto batch=update::active;const auto root=update::observedRoot;
  return enabled&&observer&&GetCurrentThreadId()==ownerThread&&batch&&root&&root->valid&&
    update::BatchCurrent(*batch)&&Context(*batch)==collectingContext&&root->sequence==batch->sequence&&
    root->scene&&root->systemRoot&&update::SceneIdentity(*root,true)&&InstalledIdentity()&&
    SameEpochs()&&update::BatchCurrent(*batch)&&Context(*batch)==collectingContext;
}
static void WriteCall(const Record& before,const Record& after,uint32_t inherited,uint32_t frame,bool accepted){
  if(!CanLog())return;
  uint32_t differences=0;
  for(unsigned i=0;i<26;++i)if(At(before.raw,0xf0+4*i)!=At(after.raw,0xf0+4*i))differences|=1u<<i;
  fprintf(output,"{\"event\":\"call\",\"frame\":%u,\"scene\":%u,\"light\":%u,\"sequence\":%llu,\"inheritedFlags\":%u,\"beforeFlags\":%u,\"afterFlags\":%u,\"payloadDifferenceMask\":%u,\"correlated\":%s}\n",
    frame,At(before.raw,0x3c),before.address,collectingContext.sequence,inherited,
    At(before.raw,0xb0),At(after.raw,0xb0),differences,accepted?"true":"false");
}
static uint32_t LightAndRestore(void* object,uint32_t flags,const Observer::Ticket* ticket,const Record* before,bool beforeReadable,uint32_t callFrame){
  uint32_t result=0;bool normal=false;
  __try {
    result=original(object,flags);normal=true;
    if(GetCurrentThreadId()==ownerThread&&beforeReadable){
      Record after{};const bool afterReadable=ReadRecord(object,after);
      const bool accepted=ticket->valid&&CallContextCurrent()&&afterReadable&&
        observer->CompleteCall(*ticket,collectingContext,after);
      if(ticket->valid){
        if(accepted)InterlockedIncrement(&correlated);
        else {InvalidateEpoch();observer->Invalidate();InterlockedIncrement(&rejected);}
      }
      if(afterReadable)WriteCall(*before,after,flags,callFrame,accepted);
    }
  } __finally {
    if(!normal){InvalidateEpoch();if(GetCurrentThreadId()==ownerThread&&observer)observer->Invalidate();}
    InterlockedDecrement(&activeLights);
  }
  return result;
}
static uint32_t __fastcall LightUpdate(void* object,void*,uint32_t flags){
  if(!enabled)return original(object,flags);
  InterlockedIncrement(&activeLights);InterlockedIncrement(&calls);
  Observer::Ticket ticket{};Record before{};
  const bool owner=GetCurrentThreadId()==ownerThread;
  const auto callFrame=owner?frameId:0u;
  const bool beforeReadable=owner&&ReadRecord(object,before);
  if(beforeReadable&&CallContextCurrent()){
    const auto root=update::observedRoot;
    ticket=observer->BeginCall(collectingContext,root->scene,root->systemRoot,before,flags);
  }
  if(!ticket.valid){
    InvalidateEpoch();InterlockedIncrement(&rejected);
    if(GetCurrentThreadId()==ownerThread&&observer)observer->Invalidate();
  }
  return LightAndRestore(object,flags,&ticket,&before,beforeReadable,callFrame);
}
static bool Install(){
  constexpr unsigned char prefix[8]={0x8b,0x44,0x24,0x04,0x53,0x56,0x8b,0xf1};
  unsigned char observed[8]{};uint32_t slot=0;
  if(!update::enabled||!update::InstalledIdentity()||update::Running()!=0||Running()!=0||
     GetCurrentThreadId()!=update::ownerThread||!update::Read(lightSlotAddress,slot)||slot!=lightEntry||
     !scene_geometry::Read(lightEntry,observed,8)||memcmp(observed,prefix,8))return false;
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,13,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
  if(!trampoline)return false;
  memcpy(trampoline,prefix,8);trampoline[8]=0xe9;
  const auto back=uint32_t(lightEntry+8-reinterpret_cast<uintptr_t>(trampoline+13));memcpy(trampoline+9,&back,4);
  DWORD old=0;
  if(!VirtualProtect(trampoline,13,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),trampoline,13)){
    VirtualFree(trampoline,0,MEM_RELEASE);return false;
  }
  if(!VirtualProtect(reinterpret_cast<void*>(lightEntry),8,PAGE_EXECUTE_READWRITE,&old)){
    VirtualFree(trampoline,0,MEM_RELEASE);return false;
  }
  DWORD ignored=0;
  const bool ready=update::Running()==0&&Running()==0&&update::InstalledIdentity()&&
    update::Read(lightSlotAddress,slot)&&slot==lightEntry&&scene_geometry::Read(lightEntry,observed,8)&&!memcmp(observed,prefix,8);
  if(!ready){VirtualProtect(reinterpret_cast<void*>(lightEntry),8,old,&ignored);VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  memset(replacement,0x90,8);replacement[0]=0xe9;
  const auto jump=uint32_t(reinterpret_cast<uintptr_t>(&LightUpdate)-lightEntry-5);memcpy(replacement+1,&jump,4);
  original=reinterpret_cast<NativeLight>(trampoline);
  // Startup-quiescent install, exactly like the enclosing Root observer.
  // Original entry is published before the non-atomic eight-byte patch. Keep
  // the trampoline for process lifetime even if final permission/flush fails.
  memcpy(reinterpret_cast<void*>(lightEntry),replacement,8);installed=true;
  const bool protectedEntry=VirtualProtect(reinterpret_cast<void*>(lightEntry),8,old,&ignored)!=FALSE;
  const bool flushed=FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(lightEntry),8)!=FALSE;
  return protectedEntry&&flushed;
}
static Observer::LookupStatus Lookup(const Record& record,Observer::Sample& sample){
  if(!enabled||!observer||GetCurrentThreadId()!=ownerThread||Running()!=0||update::Running()!=0||update::active||
     frameId!=collectingContext.frame||native_camera_source::deviceEpoch!=collectingContext.deviceEpoch||
     scene_geometry::MutationSerial()!=collectingContext.mutation||update::Serial()!=collectingContext.sequence||
     Epoch()!=collectingContext.observerEpoch||!InstalledIdentity())return Observer::LookupStatus::Unqualified;
  const update::Witness* root=nullptr;
  const auto scene=At(record.raw,0x3c);
  for(unsigned i=0;i<update::tokenCount;++i)if(update::tokens[i].scene==scene&&
    update::QualifyWitness(update::tokens[i],scene,frameId,collectingContext.mutation)){root=&update::tokens[i];break;}
  if(!root||At(record.raw,0)!=0x6f0c88)return Observer::LookupStatus::Unqualified;
  Record now{};
  if(!ReadRecord(reinterpret_cast<void*>(record.address),now))return Observer::LookupStatus::Unqualified;
  Observer::Sample candidate{};
  const auto status=observer->Lookup(collectingContext,record,candidate);
  if(Running()!=0||update::Running()!=0||Epoch()!=collectingContext.observerEpoch||update::Serial()!=collectingContext.sequence)
    return Observer::LookupStatus::Unqualified;
  if(!update::QualifyWitness(*root,scene,frameId,collectingContext.mutation))return Observer::LookupStatus::Unqualified;
  if(!SameEpochs()||Running()!=0||update::Running()!=0)return Observer::LookupStatus::Unqualified;
  if(status==Observer::LookupStatus::Current&&memcmp(now.raw,record.raw,sizeof(record.raw)))return Observer::LookupStatus::Changed;
  if(status==Observer::LookupStatus::Current&&candidate.root!=root->systemRoot)return Observer::LookupStatus::Unqualified;
  if(status==Observer::LookupStatus::Current)sample=candidate;
  return status;
}
static void ObserveScenes(){
  if(!CanLog()||!observer||!observer->Sealed())return;
  for(unsigned i=0;i<update::tokenCount;++i){
    const auto scene=update::tokens[i].scene;
    bool duplicate=false;for(unsigned j=0;j<i;++j)if(update::tokens[j].scene==scene)duplicate=true;
    if(duplicate)continue;
    try {
      const auto snapshot=winx_remix::SceneLightSnapshotSource{}.Capture(scene,
        [](uint32_t address,void* out,size_t size){return scene_geometry::Read(address,out,size);},
        [](uint32_t address,void* out,size_t size){return scene_geometry::Read(address,out,size);});
      unsigned current=0,changed=0,notObserved=0,unqualified=0,worldDirty=0,lightDirty=0;
      if(snapshot.valid)for(const auto& record:snapshot.lights){
        if(!CanLog())break;
        Observer::Sample sample{};const auto status=Lookup(record,sample);
        if(status==Observer::LookupStatus::Current)++current;
        if(status==Observer::LookupStatus::Changed)++changed;
        if(status==Observer::LookupStatus::NotObserved)++notObserved;
        if(status==Observer::LookupStatus::Unqualified)++unqualified;
        const auto flags=At(record.raw,0xb0);worldDirty+=(flags&7)!=0;lightDirty+=(flags&8)!=0;
        fprintf(output,"{\"event\":\"observation\",\"frame\":%u,\"scene\":%u,\"light\":%u,\"status\":\"%s\",\"flags\":%u,\"type\":%u,\"enabled\":%u,\"worldPositionBits\":[%u,%u,%u],\"cachedRangeBits\":%u}\n",
          frameId,scene,record.address,status==Observer::LookupStatus::Current?"Current":status==Observer::LookupStatus::Changed?"Changed":
          status==Observer::LookupStatus::NotObserved?"NotObserved":"Unqualified",flags,At(record.raw,0xc0),record.raw[0xed],
          At(record.raw,0x74),At(record.raw,0x78),At(record.raw,0x7c),At(record.raw,0x13c));
      }
      if(CanLog())fprintf(output,"{\"event\":\"scene\",\"frame\":%u,\"scene\":%u,\"snapshotValid\":%s,\"loaded\":%u,\"current\":%u,\"changed\":%u,\"notObserved\":%u,\"unqualified\":%u,\"worldDirty\":%u,\"lightDirty\":%u}\n",
        frameId,scene,snapshot.valid?"true":"false",snapshot.count,current,changed,notObserved,unqualified,worldDirty,lightDirty);
    }catch(const std::bad_alloc&){fprintf(output,"{\"event\":\"scene\",\"frame\":%u,\"scene\":%u,\"snapshotValid\":false,\"allocationFailure\":true}\n",frameId,scene);}
    if(!CanLog())break;
  }
}
#endif
static void Initialize(){
#if defined(_M_IX86)
  if(attempted)return;
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;attempted=true;output=_wfsopen(path,L"wb",_SH_DENYNO);
  const bool verified=scene_audit::VerifiedImage();
  if(verified&&update::enabled&&GetCurrentThreadId()==update::ownerThread){
    ownerThread=update::ownerThread;
    try{observer=std::make_unique<Observer>();enabled=Install();}catch(const std::bad_alloc&){}
    if(enabled){update::batchBegan=BatchBegan;update::batchFinished=BatchFinished;}
  }
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"installed\":%s,\"startupIdentityVerified\":%s,\"capacity\":512,\"scope\":\"DXLight world entry including secondary thunk; owned pre/post bytes; completed known Root and Manager correlation; no lifetime or all-producer claim\"}\n",
    enabled?"true":"false",installed?"true":"false",verified?"true":"false");fflush(output);}
#endif
}
static void EndFrame(){
#if defined(_M_IX86)
  if(!enabled)return;
  if(GetCurrentThreadId()!=ownerThread){InvalidateEpoch();return;}
  ObserveScenes();InvalidateEpoch();
  if(observer)observer->Invalidate();
  const auto frameCalls=InterlockedExchange(&calls,0),frameCorrelated=InterlockedExchange(&correlated,0),
    frameRejected=InterlockedExchange(&rejected,0),frameBatches=InterlockedExchange(&batches,0),frameSealed=InterlockedExchange(&sealed,0);
  if(CanLog()){fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"calls\":%ld,\"correlated\":%ld,\"rejected\":%ld,\"batches\":%ld,\"sealed\":%ld}\n",
    frameId,frameCalls,frameCorrelated,frameRejected,frameBatches,frameSealed);fflush(output);}
#endif
}
} // namespace native_light_world_source
