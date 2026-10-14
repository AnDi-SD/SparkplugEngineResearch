// Own CPU fixture: only owned x86 thunks and C++ mock producers execute.
// Shared root-fixture helpers retain the real enclosing detour/TLS correlation.
#define main native_update_fixture_main
#include "test_native_update.cpp"
#undef main
#include "winx_native_light_world_source.h"

namespace light_world_test {
namespace source=native_light_world_source;
namespace update=native_update_source;
using update_test::Check;
using update_test::Ptr;
using update_test::Put;
using update_test::Word;
using Observer=source::Observer;
using Record=source::Record;
using Sample=Observer::Sample;
using Status=Observer::LookupStatus;

struct LightFixture {alignas(4) unsigned char manager[0x24],lights[3][0x158];};
static LightFixture* lights;
static volatile LONG lightCalls;
static uint32_t lightArgument,lightResult=0xc7030000;
static unsigned callsPerRoot=1;
static bool distinctLights;
static thread_local unsigned lightDepth;
static HANDLE lightEntered,lightRelease;
enum class Mode {Normal,Exception,CatchException,Frame,Device,Mutation,Scene,Primary,RootParent,SceneRoot,Manager,NestedLight,ForeignBlocked};
static Mode mode=Mode::Normal;
static uint32_t Call(void* object,uint32_t flags){return reinterpret_cast<source::NativeLight>(source::lightEntry)(object,flags);}
static uint32_t __fastcall MockLight(void* object,void*,uint32_t flags){
  InterlockedIncrement(&lightCalls);
  if(flags!=lightArgument)RaiseException(0xe0421241,0,0,nullptr);
  ++lightDepth;
  uint32_t result=0;
  __try {
    if(mode==Mode::ForeignBlocked&&GetCurrentThreadId()!=source::ownerThread){
      SetEvent(lightEntered);
      if(WaitForSingleObject(lightRelease,2000)!=WAIT_OBJECT_0)RaiseException(0xe0421242,0,0,nullptr);
    }
    // Deliberately simple own producer, without claiming reconstructed logic.
    Put(object,0xb0,Word(object,0xb0)&~9u);
    Put(object,0x74,Word(object,0x74)+1);
    Put(object,0xf4,Word(object,0xf4)+1);
    if(mode==Mode::Exception||mode==Mode::CatchException)RaiseException(0xe0421240,0,0,nullptr);
    if(mode==Mode::Frame)++frameId;
    if(mode==Mode::Device)++native_camera_source::deviceEpoch;
    if(mode==Mode::Mutation)InterlockedIncrement64(&scene_geometry::mutation);
    if(mode==Mode::Scene)Put(object,0x3c,Ptr(update_test::f->scenes[1]));
    if(mode==Mode::Primary)Put(object,0,0x6f0000);
    if(mode==Mode::RootParent)Put(update_test::f->roots[0],0x2c,Ptr(update_test::f->roots[1]));
    if(mode==Mode::SceneRoot)Put(update_test::f->scenes[0],0x14,Ptr(update_test::f->roots[1]));
    if(mode==Mode::Manager)update_test::f->singleton=Ptr(update_test::f->scenes[0]);
    if(mode==Mode::NestedLight&&lightDepth==1)Call(object,flags);
    result=lightResult;
  } __finally {--lightDepth;}
  return result;
}
static bool CatchLight(void* object,uint32_t flags){
  __try {Call(object,flags);}
  __except(GetExceptionCode()==0xe0421240?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static uint32_t __fastcall MockRootWithLights(void* object,void* edx,uint32_t flags){
  const auto result=update_test::MockRoot(object,edx,flags);
  for(unsigned i=0;i<callsPerRoot;++i){
    auto* light=lights->lights[distinctLights?i:0];
    if(mode==Mode::CatchException){if(!CatchLight(light,lightArgument))RaiseException(0xe0421243,0,0,nullptr);}
    else Call(light,lightArgument);
  }
  return result;
}

struct OwnedLightCode {
  unsigned char* memory=nullptr;
  update_test::OwnedCode& root;
  static constexpr unsigned char prefix[8]={0x8b,0x44,0x24,0x04,0x53,0x56,0x8b,0xf1};
  explicit OwnedLightCode(update_test::OwnedCode& rootCode):root(rootCode){
    memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(memory!=nullptr,"owned light thunk allocation");
    memcpy(memory,prefix,sizeof(prefix));
    // Prefix leaves the inherited argument in EAX and complete object in ESI.
    // Resume invokes our fastcall mock with its one stack argument, then the
    // native-equivalent epilogue restores both saved registers and ret4.
    const unsigned char body[]={0x50,0x8b,0xce,0xb8,0,0,0,0,0xff,0xd0,0x5e,0x5b,0xc2,0x04,0};
    memcpy(memory+8,body,sizeof(body));Put(memory,12,Ptr(reinterpret_cast<void*>(&MockLight)));
    Put(memory,0x100,Ptr(memory));
    source::lightEntry=Ptr(memory);source::lightSlotAddress=Ptr(memory+0x100);
    DWORD old=0;Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)!=FALSE,"owned light page RX");
    Check(FlushInstructionCache(GetCurrentProcess(),memory,4096)!=FALSE,"flush owned light code");
    const auto rootOriginal=Ptr(reinterpret_cast<void*>(&MockRootWithLights));
    root.Write(0x100,&rootOriginal,sizeof(rootOriginal));update::rootOriginalEntry=rootOriginal;
  }
  void Write(unsigned offset,const void* bytes,unsigned size){
    DWORD old=0;Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"light fixture mutation permission");
    memcpy(memory+offset,bytes,size);DWORD ignored=0;
    Check(VirtualProtect(memory,4096,old,&ignored)!=FALSE,"light fixture mutation protection restored");
    Check(FlushInstructionCache(GetCurrentProcess(),memory+offset,size)!=FALSE,"light fixture mutation flushed");
  }
  static void RestoreNoThrow(unsigned char* target,const void* bytes,unsigned size) noexcept {
    DWORD old=0,ignored=0;
    if(VirtualProtect(target,size,PAGE_EXECUTE_READWRITE,&old)){
      memcpy(target,bytes,size);VirtualProtect(target,size,old,&ignored);FlushInstructionCache(GetCurrentProcess(),target,size);
    }
  }
  void Restore() noexcept {
    source::enabled=false;update::enabled=false;
    update::batchBegan=nullptr;update::batchFinished=nullptr;
    if(source::observer)source::observer->Invalidate();source::observer.reset();
    if(memory)RestoreNoThrow(memory,prefix,sizeof(prefix));
    constexpr unsigned char managerPrefix[7]={0x56,0x57,0x8b,0xf9,0x8b,0x47,0x18};
    RestoreNoThrow(root.memory,managerPrefix,sizeof(managerPrefix));
    const auto rootOriginal=static_cast<uint32_t>(update::rootOriginalEntry);
    RestoreNoThrow(root.memory+0x100,&rootOriginal,sizeof(rootOriginal));
  }
  ~OwnedLightCode(){
    Restore();
    if(source::output){fclose(source::output);source::output=nullptr;}
    if(update::output){fclose(update::output);update::output=nullptr;}
    if(source::original)VirtualFree(reinterpret_cast<void*>(source::original),0,MEM_RELEASE);
    source::original=nullptr;source::installed=false;
    if(memory)VirtualFree(memory,0,MEM_RELEASE);
  }
};

static void ClearAttempt(){
  Check(!source::installed,"rejected install never published an entry patch");
  source::enabled=false;source::attempted=false;
  if(source::output){fclose(source::output);source::output=nullptr;}
  update::batchBegan=nullptr;update::batchFinished=nullptr;source::observer.reset();
}
static void Install(OwnedLightCode& code){
  unsigned char before[0x104];memcpy(before,code.memory,sizeof(before));
  const auto serial=update::Serial();const auto queries=update::queries;
  const auto verifies=scene_audit::verifies;
  scene_audit::verified=false;SetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",L"light-image-reject.jsonl");source::Initialize();
  Check(!source::enabled&&scene_audit::verifies==verifies+1&&!memcmp(before,code.memory,sizeof(before)),"full image guard blocks light patch");ClearAttempt();
  scene_audit::verified=true;update::enabled=false;
  SetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",L"light-root-reject.jsonl");source::Initialize();
  Check(!source::enabled&&!memcmp(before,code.memory,sizeof(before)),"light observer requires installed enclosing root source");ClearAttempt();update::enabled=true;
  unsigned char bad=0x90;code.Write(0,&bad,1);
  SetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",L"light-prefix-reject.jsonl");source::Initialize();
  Check(!source::enabled&&Word(code.memory,0x100)==source::lightEntry,"bad prefix does not alter original light slot");ClearAttempt();code.Write(0,before,1);
  const auto badSlot=static_cast<uint32_t>(source::lightEntry)+1;code.Write(0x100,&badSlot,sizeof(badSlot));
  SetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",L"light-slot-reject.jsonl");source::Initialize();
  Check(!source::enabled&&!memcmp(before,code.memory,23),"bad light slot leaves entry unchanged");ClearAttempt();code.Write(0x100,before+0x100,4);
  SetEnvironmentVariableW(L"WINX_REMIX_LIGHT_WORLD_OBSERVER",L"light-world.jsonl");source::Initialize();
  Check(source::enabled&&source::installed&&source::InstalledIdentity(),"verified light entry installed on owned memory");
  Check(update::batchBegan==source::BatchBegan&&update::batchFinished==source::BatchFinished,"passive batch callbacks installed");
  Check(update::Serial()==serial&&update::queries==queries,"light install preserves enclosing root serial/accounting");
  MEMORY_BASIC_INFORMATION region{};VirtualQuery(code.memory,&region,sizeof(region));Check(region.Protect==PAGE_EXECUTE_READ,"patched light page restored RX");
  const auto verifiedCalls=scene_audit::verifies;source::Initialize();
  Check(source::InstalledIdentity()&&scene_audit::verifies==verifiedCalls,"light initialization is idempotent");
}
static void Reset(){
  update_test::Reset();memset(lights,0,sizeof(*lights));
  const auto scene=Ptr(update_test::f->scenes[0]);
  Put(lights->manager,0,0x6e8ca4);Put(lights->manager,0x10,Ptr(lights->lights[0]));
  Put(lights->manager,0x14,Ptr(lights->lights[2]));Put(lights->manager,0x18,3);Put(lights->manager,0x20,scene);
  Put(update_test::f->scenes[0],0x34,Ptr(lights->manager));
  for(unsigned i=0;i<3;++i){
    auto& light=lights->lights[i];Put(light,0,0x6f0c88);Put(light,0x3c,scene);Put(light,0xb0,0x109);Put(light,0xf0,1);
    Put(light,0xb8,i?Ptr(lights->lights[i-1]):0);Put(light,0xbc,i<2?Ptr(lights->lights[i+1]):0);
  }
  source::EndFrame();mode=Mode::Normal;lightArgument=0;lightResult=0xc7030000;callsPerRoot=1;distinctLights=false;
}
static Record Capture(unsigned i=0){Record record{};record.address=Ptr(lights->lights[i]);memcpy(record.raw,lights->lights[i],sizeof(record.raw));return record;}
static Status Lookup(Sample& sample,unsigned i=0){return source::Lookup(Capture(i),sample);}
static void CheckRestored(){
  Check(source::Running()==0&&update::Running()==0&&update::active==nullptr&&update::observedRoot==nullptr&&update::rootDepth==0&&lightDepth==0,"light/root/manager scopes restored");
}
static bool CatchManager(){
  __try {update_test::Run();}
  __except(GetExceptionCode()==0xe0421240?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static uint32_t RegisterCall(void* object,uint32_t flags){
  uintptr_t function=source::lightEntry;uint32_t before=0,after=0,savedEbx=0,savedEsi=0,result=0;
  __asm {
    push ebx
    push esi
    mov ebx,0x1234abcd
    mov esi,0x5678dcba
    mov before,esp
    push flags
    mov ecx,object
    call function
    mov result,eax
    mov savedEbx,ebx
    mov savedEsi,esi
    mov after,esp
    pop esi
    pop ebx
  }
  Check(before==after&&savedEbx==0x1234abcd&&savedEsi==0x5678dcba,"light detour preserves ret4/ESP and EBX/ESI");
  return result;
}
static void SuccessAndPassivity(OwnedLightCode& code){
  Reset();lightArgument=0xfedcba98u;const auto calls=lightCalls,managerCalls=update_test::managerCalls,rootCalls=update_test::rootCalls;
  const auto initial=Capture();Check(update_test::Run()==update_test::managerResult,"manager opaque EAX unchanged by light observer");
  Check(lightCalls==calls+1&&update_test::managerCalls==managerCalls+1&&update_test::rootCalls==rootCalls+1,"light/root/manager originals each execute once");
  Sample sample{};Check(Lookup(sample)==Status::Current,"normal light return publishes after enclosing root and manager");
  Check(sample.before.address==initial.address&&!memcmp(sample.before.raw,initial.raw,sizeof(initial.raw))&&!memcmp(sample.after.raw,lights->lights[0],sizeof(initial.raw)),"published pre/post snapshots own exact producer bytes");
  Check(sample.inheritedFlags==lightArgument&&sample.calls==1&&sample.completed&&sample.root==Ptr(update_test::f->roots[0])&&sample.scene==Ptr(update_test::f->scenes[0]),"full inherited flags and root/scene correlation retained");
  Check(Word(sample.before.raw,0xb0)==0x109&&Word(sample.after.raw,0xb0)==0x100&&Word(sample.after.raw,0xf4)==1,"producer dirty/payload changes observed without repair");CheckRestored();
  const auto queries=update::queries,accepted=update::accepted;const auto serial=update::Serial();Sample again{};
  Check(Lookup(again)==Status::Current&&update::queries==queries&&update::accepted==accepted&&update::Serial()==serial,"lookup leaves root accounting/serial unchanged");
  Check(Lookup(again,2)==Status::NotObserved,"valid unvisited light reports only NotObserved");
  const auto oldSample=sample;const auto cachedRecord=Capture();Put(lights->lights[0],0xf4,99);
  Check(source::Lookup(cachedRecord,again)==Status::Changed,"cached caller snapshot cannot claim Current after live light bytes changed");
  Check(Lookup(again)==Status::Changed&&!memcmp(sample.after.raw,oldSample.after.raw,sizeof(sample.after.raw))&&!memcmp(sample.before.raw,oldSample.before.raw,sizeof(sample.before.raw)),"changed bytes reject current lookup while owned sample stays unchanged");
  Put(lights->lights[0],0xf4,1);Check(Lookup(again)==Status::Current,"identical restored post bytes can match within qualified phase");
  ++frameId;Check(Lookup(again)==Status::Unqualified,"later frame rejects observation");--frameId;
  ++native_camera_source::deviceEpoch;Check(Lookup(again)==Status::Unqualified,"device reset rejects observation");--native_camera_source::deviceEpoch;
  InterlockedIncrement64(&scene_geometry::mutation);Check(Lookup(again)==Status::Unqualified,"native retirement mutation rejects observation");

  Reset();update_test::Run();Check(Lookup(sample)==Status::Current,"fresh update recovers after stale phase");
  const unsigned char installed=code.memory[0],bad=0x90;code.Write(0,&bad,1);
  Check(Lookup(again)==Status::Unqualified,"entry tamper rejects lookup");code.Write(0,&installed,1);
  const auto originalSlot=static_cast<uint32_t>(source::lightEntry),badSlot=originalSlot+4;code.Write(0x100,&badSlot,4);
  Check(Lookup(again)==Status::Unqualified,"light virtual slot tamper rejects lookup");code.Write(0x100,&originalSlot,4);
  Put(update_test::f->roots[0],0x2c,Ptr(update_test::f->roots[1]));Check(Lookup(again)==Status::Unqualified,"late root parent change rejects borrowed scene identity");Put(update_test::f->roots[0],0x2c,0);
  Put(update_test::f->scenes[0],0x14,Ptr(update_test::f->roots[1]));Check(Lookup(again)==Status::Unqualified,"late scene system-root replacement rejects lookup");Put(update_test::f->scenes[0],0x14,Ptr(update_test::f->roots[0]));
  update_test::f->singleton=Ptr(update_test::f->scenes[0]);Check(Lookup(again)==Status::Unqualified,"late manager singleton replacement rejects lookup");update_test::f->singleton=Ptr(update_test::f->manager);
  source::EndFrame();Check(Lookup(again)==Status::Unqualified,"observer EndFrame revokes same-frame evidence");

  Reset();callsPerRoot=2;const auto twice=lightCalls;update_test::Run();
  Check(lightCalls==twice+2&&Lookup(sample)==Status::Current&&sample.calls==2&&sample.call==2&&Word(sample.before.raw,0xf4)==1&&Word(sample.after.raw,0xf4)==2,"repeated completed same-key calls keep latest snapshot and count");
  const auto previous=sample;const auto beforeNewBatch=lightCalls;update_test::Run();
  Check(lightCalls==beforeNewBatch+2&&Lookup(again)==Status::Current&&again.calls==2&&Word(again.before.raw,0xf4)==3&&Word(again.after.raw,0xf4)==4,"later same-frame batch replaces old samples with fresh tickets");
  Check(Word(previous.after.raw,0xf4)==2,"previous output owns immutable sample bytes");

  Reset();source::enabled=false;update_test::Run();const auto nativeOnly=*lights;
  Reset();source::enabled=true;update_test::Run();
  Check(!memcmp(&nativeOnly,lights,sizeof(nativeOnly))&&Lookup(sample)==Status::Current,"observer enabled/disabled leaves byte-identical native producer state");
  for(const uint32_t eax:{0u,0x81020300u,0xffffffffu}){
    Reset();lightArgument=0xfedcba98u;lightResult=eax;update_test::Run();Check(Lookup(sample)==Status::Current,"zero/AL-zero opaque return still means normal completion");
    const auto beforeCall=lightCalls;const auto rootSerial=update::Serial();update::Witness witness{};Check(update_test::Get(0,witness),"root witness before standalone register check");
    const auto rootQueries=update::queries,rootAccepted=update::accepted;
    Check(RegisterCall(lights->lights[0],lightArgument)==eax&&lightCalls==beforeCall+1,"light detour preserves full opaque EAX and exact one original call");
    Check(Lookup(sample)==Status::Unqualified&&update::Serial()==rootSerial&&update::queries==rootQueries&&update::accepted==rootAccepted&&update::QualifyWitness(witness,witness.scene,witness.frame,witness.mutation),"standalone light revokes only light evidence while root witness/accounting remain current");CheckRestored();
  }
}
static unsigned Occurrences(const char* text,const char* token){
  unsigned count=0;const auto size=strlen(token);
  while((text=strstr(text,token))!=nullptr){++count;text+=size;}
  return count;
}
static void FrameCensus(){
  Reset();callsPerRoot=2;distinctLights=true;const auto beforeCalls=lightCalls;
  // Authored, cached world and cached DX ranges deliberately differ. Census
  // copies the DX word; it must not infer a range or replay its producer.
  Put(lights->lights[0],0xc4,0x3fa00000);Put(lights->lights[0],0xe0,0x40200000);
  Put(lights->lights[0],0x13c,0x40700000);
  update_test::Run();Sample sample{};
  Check(lightCalls==beforeCalls+2&&Lookup(sample,0)==Status::Current&&Lookup(sample,1)==Status::Current&&Lookup(sample,2)==Status::NotObserved,"two distinct completed producers qualify before loaded scene census");
  Put(lights->lights[1],0xf4,99);
  Check(Lookup(sample,1)==Status::Changed,"census changed-light input reached after completed producer");
  update::Witness witness{};Check(update_test::Get(0,witness),"core root witness available before observer census");
  const auto serial=update::Serial(),epoch=source::Epoch();const auto queries=update::queries,accepted=update::accepted;
  Check(source::output!=nullptr,"census log open");const auto begin=_ftelli64(source::output);Check(begin>=0,"census log offset");
  source::EndFrame();Check(fflush(source::output)==0,"census log flushed for bounded read");
  Check(update::Serial()==serial&&update::queries==queries&&update::accepted==accepted&&update::QualifyWitness(witness,witness.scene,witness.frame,witness.mutation),"scene census preserves old core serial/accounting and borrowed root witness");
  Check(source::Epoch()>epoch&&Lookup(sample)==Status::Unqualified&&source::observer->Count()==0,"EndFrame revokes only completed light observation after recording census");
  constexpr size_t limit=64*1024;auto bytes=std::make_unique<char[]>(limit+1);
  FILE* input=_wfsopen(L"light-world.jsonl",L"rb",_SH_DENYNO);Check(input!=nullptr,"bounded census log reader");
  const bool seeked=_fseeki64(input,begin,SEEK_SET)==0;
  const auto size=seeked?fread(bytes.get(),1,limit,input):0;const bool readOk=!ferror(input);fclose(input);bytes[size]=0;
  Check(seeked&&readOk&&size>0&&size<limit,"census log read stays below 64 KiB");
  const auto summary=strstr(bytes.get(),"\"event\":\"scene\"");
  Check(summary&&strstr(summary,"\"snapshotValid\":true,\"loaded\":3,\"current\":1,\"changed\":1,\"notObserved\":1,\"unqualified\":0")!=nullptr,"end-frame registry census reports Current/Changed/NotObserved one each and valid loaded three");
  Check(Occurrences(bytes.get(),"\"event\":\"observation\"")==3&&Occurrences(bytes.get(),"\"status\":\"Current\"")==1&&Occurrences(bytes.get(),"\"status\":\"Changed\"")==1&&Occurrences(bytes.get(),"\"status\":\"NotObserved\"")==1,"census emits one owned classification line per loaded light");
  Check(Occurrences(bytes.get(),"\"cachedRangeBits\"")==3&&strstr(bytes.get(),"\"cachedRangeBits\":1081081856")!=nullptr,"census copies cached DX range bits rather than authored or cached world range");
  Check(Word(lights->lights[0],0xc4)==0x3fa00000&&Word(lights->lights[0],0xe0)==0x40200000&&Word(lights->lights[0],0x13c)==0x40700000,"range census preserves all native input/cache bytes");
  CheckRestored();
}
static void RejectMode(Mode rejected,const char* message){
  Reset();mode=rejected;const auto beforeCalls=lightCalls,beforeRoots=update_test::rootCalls,beforeManagers=update_test::managerCalls;
  update_test::Run();Sample sample{};
  Check(lightCalls==beforeCalls+1&&update_test::rootCalls==beforeRoots+1&&update_test::managerCalls==beforeManagers+1,"rejected producer boundary reached once without replay");
  Check(Lookup(sample)==Status::Unqualified,message);CheckRestored();
}
static DWORD WINAPI ForeignLight(void*){Call(lights->lights[1],lightArgument);return 0;}
static void FailuresAndThreads(){
  RejectMode(Mode::Frame,"frame mutation during light producer rejects publication");
  RejectMode(Mode::Device,"device mutation during light producer rejects publication");
  RejectMode(Mode::Mutation,"retirement mutation during light producer rejects publication");
  RejectMode(Mode::Scene,"light owner scene mutation during producer rejects publication");
  RejectMode(Mode::Primary,"light primary mutation during producer rejects publication");
  RejectMode(Mode::RootParent,"root parent mutation during light producer rejects publication");
  RejectMode(Mode::SceneRoot,"scene root mutation during light producer rejects publication");
  RejectMode(Mode::Manager,"manager singleton mutation during light producer rejects publication");
  Sample sample{};
  Reset();update_test::Run();Check(Lookup(sample)==Status::Current,"baseline before later light exception");mode=Mode::Exception;
  const auto thrown=lightCalls;Check(CatchManager()&&lightCalls==thrown+1,"original light SEH propagates once through all detours");
  Check(Lookup(sample)==Status::Unqualified&&Word(update_test::f->manager,0x20)==Ptr(update_test::f->scenes[0]),"exception revokes previous success without repairing native manager currentScene");CheckRestored();
  Reset();mode=Mode::CatchException;const auto caught=lightCalls;update_test::Run();
  Check(lightCalls==caught+1&&Lookup(sample)==Status::Unqualified,"caught light SEH cannot publish from normal containing manager return");
  update::Witness witness{};Check(update_test::Get(0,witness),"caught light SEH leaves original plain root qualification unchanged");CheckRestored();
  Reset();mode=Mode::NestedLight;const auto nested=lightCalls;update_test::Run();
  Check(lightCalls==nested+2&&Lookup(sample)==Status::Unqualified,"nested same-light originals run once each while overlapping observation rejects");
  Check(update_test::Get(0,witness),"nested light invalidation does not revoke unrelated root witness");CheckRestored();
  Reset();update_test::mode=update_test::Mode::Reenter;const auto nestedManager=update_test::managerCalls,lightBefore=lightCalls;update_test::Run();
  Check(update_test::managerCalls==nestedManager+2&&lightCalls==lightBefore+1&&Lookup(sample)==Status::Unqualified,"nested manager executes originals and revokes containing observation");CheckRestored();

  Reset();update_test::Run();Check(Lookup(sample)==Status::Current&&update_test::Get(0,witness),"baseline before foreign standalone light");
  const auto serial=update::Serial();const auto queries=update::queries,accepted=update::accepted;const auto foreignBefore=lightCalls;
  auto thread=CreateThread(nullptr,0,ForeignLight,nullptr,0,nullptr);
  Check(thread&&WaitForSingleObject(thread,2000)==WAIT_OBJECT_0,"foreign standalone light completes");CloseHandle(thread);
  Check(lightCalls==foreignBefore+1&&Lookup(sample)==Status::Unqualified&&update::Serial()==serial&&update::queries==queries&&update::accepted==accepted&&update::QualifyWitness(witness,witness.scene,witness.frame,witness.mutation),"foreign light revokes only observer evidence without touching old root counters/serial");CheckRestored();

  Reset();mode=Mode::ForeignBlocked;lightEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);lightRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  Check(lightEntered&&lightRelease,"owned foreign light synchronization events");const auto concurrent=lightCalls;
  thread=CreateThread(nullptr,0,ForeignLight,nullptr,0,nullptr);
  Check(thread&&WaitForSingleObject(lightEntered,2000)==WAIT_OBJECT_0&&source::Running()==1,"foreign producer entered original and stays in flight");
  update_test::Run();Check(lightCalls==concurrent+2&&Lookup(sample)==Status::Unqualified&&source::Running()==1,"owner manager cannot publish across an in-flight foreign light");
  SetEvent(lightRelease);Check(WaitForSingleObject(thread,2000)==WAIT_OBJECT_0,"foreign producer released");CloseHandle(thread);CloseHandle(lightEntered);CloseHandle(lightRelease);
  Check(Lookup(sample)==Status::Unqualified,"late foreign completion cannot revive owner observation");CheckRestored();
  Reset();update_test::Run();Check(Lookup(sample)==Status::Current,"fresh normal batch recovers after exceptional/reentrant/foreign observations");
}
}

int main(){
  using namespace light_world_test;
  static_assert(sizeof(void*)==4,"x86 owned ABI fixture required");
  try{
    update_test::Watchdog watchdog;
    auto fixture=std::make_unique<update_test::Fixture>();update_test::f=fixture.get();fixture->Reset();
    auto lightFixture=std::make_unique<LightFixture>();lights=lightFixture.get();
    update_test::OwnedCode rootCode;OwnedLightCode lightCode(rootCode);update_test::Install(rootCode);Install(lightCode);
    SuccessAndPassivity(lightCode);FrameCensus();FailuresAndThreads();source::EndFrame();update::EndFrame();
    lightCode.Restore();
    Check(!memcmp(lightCode.memory,OwnedLightCode::prefix,8)&&Word(rootCode.memory,0x100)==update::rootOriginalEntry&&rootCode.memory[0]==0x56,"owned light/root detours restored before code and trampolines are freed");
    Check(!update::batchBegan&&!update::batchFinished&&!source::observer,"callbacks disconnected before observer destruction");
    if(update::output){fclose(update::output);update::output=nullptr;}
    printf("{\"status\":\"PASS\",\"checks\":%u,\"lightCalls\":%ld,\"managerCalls\":%ld,\"rootCalls\":%ld,\"nativeGameCodeExecuted\":false,\"gpu\":false,\"ownedX86Thunks\":true}\n",update_test::checks,lightCalls,update_test::managerCalls,update_test::rootCalls);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL after %u checks: %s\n",update_test::checks,error.what());return 1;}
}
