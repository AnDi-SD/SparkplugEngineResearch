// Own CPU fixture. It executes only owned x86 thunks and C++ mock producers.
// No original executable/protected code, D3D device, bridge, game or GPU runs.
#define WINX_REMIX_TEST
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <share.h>
#include <stdexcept>
#include <memory>
#include <type_traits>
static unsigned frameId=0;
namespace scene_geometry {
__declspec(align(8)) static volatile LONG64 mutation=1;
static uint64_t MutationSerial(){return static_cast<uint64_t>(InterlockedCompareExchange64(&mutation,0,0));}
// Fixture platform dependency: guarded reads of owned records, matching the
// production helper's address/size/SEH limits. No fixture game logic is here.
static bool Read(uintptr_t address,void* data,size_t size){
  if(address<0x10000||address>=0x7fff0000||size>16384||size>0x7fff0000-address)return false;
  __try{memcpy(data,reinterpret_cast<void*>(address),size);}
  __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION||GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
  return true;
}
static uint32_t Word(uintptr_t address){uint32_t value=0;Read(address,&value,4);return value;}
}
namespace native_camera_source {static uint64_t deviceEpoch=1;}
namespace scene_audit {static bool verified=true;static unsigned verifies;static bool VerifiedImage(){++verifies;return verified;}}
#include "winx_native_update_source.h"

namespace update_test {
namespace source=native_update_source;
static unsigned checks;
static void Check(bool result,const char* message){++checks;if(!result)throw std::runtime_error(message);}
static uint32_t Ptr(const void* p){return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));}
static void Put(void* p,unsigned offset,uint32_t value){memcpy(static_cast<uint8_t*>(p)+offset,&value,4);}
static uint32_t Word(const void* p,unsigned offset){uint32_t value=0;memcpy(&value,static_cast<const uint8_t*>(p)+offset,4);return value;}
struct Watchdog {
  HANDLE stop=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* event){if(WaitForSingleObject(static_cast<HANDLE>(event),25000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0525530);return 0;}
  Watchdog(){stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(stop!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,stop,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
  ~Watchdog(){SetEvent(stop);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(stop);}
};
struct Fixture {
  alignas(4) uint8_t manager[0x24],scenes[65][0x54],roots[65][0xb4],systems[65][0x1d8],partitions[65][0x84];
  uint32_t singleton;
  void Reset(){
    memset(this,0,sizeof(*this));Put(manager,0,0x6e7154);singleton=Ptr(manager);
    for(unsigned i=0;i<65;++i){Put(scenes[i],0,0x6e7358);Put(scenes[i],0x14,Ptr(roots[i]));Put(scenes[i],0x38,Ptr(systems[i]));
      Put(roots[i],0,0x6dc4f4);Put(roots[i],0x3c,Ptr(scenes[i]));Put(roots[i],0xb0,0x70a07);
      Put(systems[i],0x1d4,Ptr(partitions[i]));}
  }
};
static Fixture* f;
static source::NativeRoot callRoot;
static unsigned sceneCount=1,managerDepth;
static uint32_t managerResult=0xa5010000,rootResult=0xb6020000,rootArgument;
static volatile LONG managerCalls,rootCalls;
enum class Mode {Normal,ForeignCaller,Reenter,RootException,ManagerException,ManagerCatchesRootException,
  Frame,Mutation,Device,SceneChange,RootChange,SystemChange,ManagerNotCleared,ForeignConcurrent,Descendant,ForeignRootBlocked};
static Mode mode=Mode::Normal;
static HANDLE foreignEntered,foreignRelease;
static uint32_t __fastcall MockManager(void*,void*);
static uint32_t __fastcall MockRoot(void*,void*,uint32_t);
static DWORD WINAPI ForeignManager(void*){source::ManagerUpdate(f->manager,nullptr);return 0;}
static DWORD WINAPI ForeignRoot(void*){source::RootUpdate(f->roots[1],nullptr,0);return 0;}
static bool CatchRoot(void* root){
  __try{callRoot(root,0);}
  __except(GetExceptionCode()==0xe0421234?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static uint32_t __fastcall MockRoot(void* object,void*,uint32_t flags){
  InterlockedIncrement(&rootCalls);
  if(flags!=rootArgument)RaiseException(0xe0421235,0,0,nullptr);
  if(mode==Mode::ForeignRootBlocked&&GetCurrentThreadId()!=source::ownerThread){
    SetEvent(foreignEntered);if(WaitForSingleObject(foreignRelease,2000)!=WAIT_OBJECT_0)RaiseException(0xe0421236,0,0,nullptr);
  }
  Put(object,0x74,Word(object,0x74)+1);Put(object,0xb0,Word(object,0xb0)&~7u);
  if(mode==Mode::RootException||mode==Mode::ManagerCatchesRootException)RaiseException(0xe0421234,0,0,nullptr);
  if(mode==Mode::Frame)++frameId;
  if(mode==Mode::Mutation)InterlockedIncrement64(&scene_geometry::mutation);
  if(mode==Mode::Device)++native_camera_source::deviceEpoch;
  if(mode==Mode::SceneChange)Put(f->manager,0x20,Ptr(f->scenes[1]));
  if(mode==Mode::RootChange)Put(f->scenes[0],0x14,Ptr(f->roots[1]));
  if(mode==Mode::SystemChange)Put(f->systems[0],0x1d4,Ptr(f->partitions[1]));
  if(mode==Mode::ForeignConcurrent){auto t=CreateThread(nullptr,0,ForeignManager,nullptr,0,nullptr);
    if(!t||WaitForSingleObject(t,2000)!=WAIT_OBJECT_0)RaiseException(0xe0421236,0,0,nullptr);CloseHandle(t);}
  if(mode==Mode::Descendant&&source::rootDepth==1){
    // An actual nested world call uses the same owned return thunk, yet cannot
    // masquerade as the manager's outer root because the wrapper depth is two.
    callRoot(f->roots[1],flags);
  }
  return rootResult;
}
static uint32_t __fastcall MockManager(void* object,void*){
  InterlockedIncrement(&managerCalls);
  if(GetCurrentThreadId()!=source::ownerThread)return managerResult;
  ++managerDepth;
  if(managerDepth>1){--managerDepth;return managerResult;}
  if(mode==Mode::ManagerException){--managerDepth;RaiseException(0xe0421234,0,0,nullptr);}
  for(unsigned i=0;i<sceneCount;++i){
    Put(object,0x20,Ptr(f->scenes[i]));
    if(mode==Mode::ForeignCaller)source::RootUpdate(f->roots[i],nullptr,rootArgument);
    else if(mode==Mode::ManagerCatchesRootException)CatchRoot(f->roots[i]);
    else callRoot(f->roots[i],rootArgument);
  }
  if(mode==Mode::Reenter)source::ManagerUpdate(object,nullptr);
  if(mode!=Mode::ManagerNotCleared)Put(object,0x20,0);
  --managerDepth;return managerResult;
}
struct OwnedCode {
  uint8_t* memory=nullptr;
  OwnedCode(){
    memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));Check(memory!=nullptr,"owned executable fixture allocation");
    const uint8_t prefix[]={0x56,0x57,0x8b,0xf9,0x8b,0x47,0x18};memcpy(memory,prefix,7);
    // Native-equivalent prologue + call own producer + matching epilogue.
    memory[7]=0x8b;memory[8]=0xcf;memory[9]=0xb8;Put(memory,10,Ptr(reinterpret_cast<void*>(&MockManager)));
    memory[14]=0xff;memory[15]=0xd0;memory[16]=0x5f;memory[17]=0x5e;memory[18]=0xc3;
    auto t=memory+0x80;const uint8_t push[]={0xff,0x74,0x24,0x04};memcpy(t,push,4);
    t[4]=0xa1;Put(t,5,Ptr(memory+0x100));t[9]=0xff;t[10]=0xd0;t[11]=0xc2;t[12]=4;t[13]=0;
    Put(memory,0x100,Ptr(reinterpret_cast<void*>(&MockRoot)));
    source::managerEntry=Ptr(memory);source::rootSlotAddress=Ptr(memory+0x100);
    source::rootOriginalEntry=Ptr(reinterpret_cast<void*>(&MockRoot));source::rootReturnAddress=Ptr(t+11);
    source::managerPointerAddress=reinterpret_cast<uintptr_t>(&f->singleton);callRoot=reinterpret_cast<source::NativeRoot>(t);
    DWORD old=0;Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)!=FALSE,"protect owned fixture page RX");
    FlushInstructionCache(GetCurrentProcess(),memory,4096);
  }
  void Write(unsigned offset,const void* bytes,unsigned size){DWORD old=0;Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"fixture mutation permission");
    memcpy(memory+offset,bytes,size);DWORD ignored=0;Check(VirtualProtect(memory,4096,old,&ignored)!=FALSE,"fixture mutation restore");FlushInstructionCache(GetCurrentProcess(),memory+offset,size);}
  ~OwnedCode(){source::enabled=false;if(source::originalManager)VirtualFree(reinterpret_cast<void*>(source::originalManager),0,MEM_RELEASE);
    source::originalManager=nullptr;source::originalRoot=nullptr;if(memory)VirtualFree(memory,0,MEM_RELEASE);}
};
static void ClearAttempt(){if(source::output){fclose(source::output);source::output=nullptr;}source::attempted=false;source::enabled=false;}
static void Install(OwnedCode& code){
  unsigned char before[0x104];memcpy(before,code.memory,sizeof(before));
  SetEnvironmentVariableW(L"WINX_REMIX_NATIVE_UPDATE_SOURCE",L"init-image-reject.jsonl");scene_audit::verified=false;
  source::Initialize();Check(!source::enabled&&scene_audit::verifies==1&&!memcmp(before,code.memory,sizeof(before)),"full image guard blocks both patches");ClearAttempt();
  scene_audit::verified=true;
  unsigned char bad=0x90;code.Write(0,&bad,1);SetEnvironmentVariableW(L"WINX_REMIX_NATIVE_UPDATE_SOURCE",L"init-prefix-reject.jsonl");source::Initialize();
  Check(!source::enabled&&Word(code.memory,0x100)==source::rootOriginalEntry,"bad manager prefix leaves root slot unchanged");ClearAttempt();code.Write(0,before,1);
  uint32_t badSlot=0x12345678;code.Write(0x100,&badSlot,4);SetEnvironmentVariableW(L"WINX_REMIX_NATIVE_UPDATE_SOURCE",L"init-slot-reject.jsonl");source::Initialize();
  Check(!source::enabled&&!memcmp(before,code.memory,19),"bad root slot leaves manager entry unchanged");ClearAttempt();code.Write(0x100,before+0x100,4);
  SetEnvironmentVariableW(L"WINX_REMIX_NATIVE_UPDATE_SOURCE",L"update.jsonl");source::Initialize();
  Check(source::enabled&&source::InstalledIdentity(),"verified pair installed on owned memory");
  MEMORY_BASIC_INFORMATION region{};VirtualQuery(code.memory,&region,sizeof(region));Check(region.Protect==PAGE_EXECUTE_READ,"overlapping page protections restored in reverse order");
  Check(source::IsPlainWorldSlot(static_cast<uint32_t>(source::rootOriginalEntry))&&source::IsPlainWorldSlot(Ptr(reinterpret_cast<void*>(&source::RootUpdate))),"plain slot recognizes original and active installed wrapper");
  Check(!source::IsPlainWorldSlot(0x12345678),"unknown world slot rejected");
  const auto verifies=scene_audit::verifies;source::Initialize();Check(scene_audit::verifies==verifies&&source::InstalledIdentity(),"Initialize is idempotent");
}
static void Reset(){
  f->Reset();mode=Mode::Normal;sceneCount=1;managerDepth=0;rootArgument=0;managerResult=0xa5010000;rootResult=0xb6020000;
  frameId=7;native_camera_source::deviceEpoch=1;InterlockedIncrement64(&scene_geometry::mutation);source::Invalidate();
}
static uint32_t Run(){return reinterpret_cast<source::NativeManager>(source::managerEntry)(f->manager);}
static bool Get(unsigned i,source::Witness& w){return source::GetWitness(Ptr(f->scenes[i]),frameId,scene_geometry::MutationSerial(),w);}
static void RejectMode(Mode m,const char* message){Reset();mode=m;const auto mc=managerCalls,rc=rootCalls;Run();source::Witness w{};
  Check(!Get(0,w)&&!w.valid,message);Check(managerCalls==mc+1&&rootCalls==rc+1,"rejected observation preserves original counts");
  Check(source::active==nullptr&&source::Running()==0&&source::rootDepth==0,"rejected observation restores TLS/running count");}
static bool CatchManager(){
  __try{Run();}
  __except(GetExceptionCode()==0xe0421234?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static void NormalAndGuards(OwnedCode& code){
  Reset();const auto mc=managerCalls,rc=rootCalls;Check(Run()==managerResult,"installed manager trampoline preserves full EAX with AL zero");
  Check(managerCalls==mc+1&&rootCalls==rc+1&&Word(f->roots[0],0x74)==1,"original manager/root execute once; producer writes happen once");
  source::Witness w{};Check(Get(0,w)&&w.valid,"normal root completion publishes despite root AL zero");
  Check(w.scene==Ptr(f->scenes[0])&&w.manager==Ptr(f->manager)&&w.systemRoot==Ptr(f->roots[0])&&w.system==Ptr(f->systems[0])&&w.root==Ptr(f->partitions[0]),"witness identities distinguish system and partition roots");
  Check(source::active==nullptr&&source::Running()==0&&source::rootDepth==0,"normal scopes restored");
  const auto queries=source::queries;Check(source::QualifyWitness(w,w.scene,w.frame,w.mutation)&&source::queries==queries,"pure qualifier does not change accounting");
  source::Witness out{};out.valid=true;
  Check(!source::GetWitness(w.scene,frameId+1,w.mutation,out)&&!out.valid,"wrong requested frame rejects and clears output");
  Check(!source::QualifyWitness(w,w.scene+4,w.frame,w.mutation),"foreign scene rejected");
  Check(!source::QualifyWitness(w,w.scene,w.frame,w.mutation+1),"foreign mutation rejected");
  ++frameId;Check(!source::QualifyWitness(w,w.scene,w.frame,w.mutation),"actual frame advance rejects old witness");--frameId;
  ++native_camera_source::deviceEpoch;Check(!source::QualifyWitness(w,w.scene,w.frame,w.mutation),"reset/device epoch rejects old witness");--native_camera_source::deviceEpoch;
  Put(f->roots[0],0x2c,Ptr(f->roots[1]));Check(!Get(0,out),"system root with new parent rejected");Put(f->roots[0],0x2c,0);
  Put(f->roots[0],0,0x6dcaa4);Check(!Get(0,out),"derived system root primary rejected");Put(f->roots[0],0,0x6dc4f4);
  Put(f->scenes[0],0x14,Ptr(f->roots[1]));Check(!Get(0,out),"late scene root replacement rejected");Put(f->scenes[0],0x14,Ptr(f->roots[0]));
  f->singleton=Ptr(f->scenes[0]);Check(!Get(0,out),"late manager singleton replacement rejected");f->singleton=Ptr(f->manager);
  unsigned char installed=code.memory[0],bad=0x90;code.Write(0,&bad,1);
  Check(!Get(0,out)&&!source::IsPlainWorldSlot(Ptr(reinterpret_cast<void*>(&source::RootUpdate))),"installed identity tamper rejects witness and wrapper identity");code.Write(0,&installed,1);
  const auto previous=w.sequence;Run();Check(Get(0,out)&&out.sequence>previous&&!source::QualifyWitness(w,w.scene,w.frame,w.mutation),"second update within same frame invalidates first serial");
  source::EndFrame();Check(!Get(0,out)&&!source::QualifyWitness(w,w.scene,w.frame,w.mutation),"EndFrame invalidates even before frame counter advances");
  Reset();sceneCount=0;Run();Check(!Get(0,out)&&source::tokenCount==0,"empty manager completes without scene tokens");
  Reset();rootResult=0;Run();Check(Get(0,out),"entire zero root EAX is still a normal completion");
  Reset();rootArgument=0xfedcba98u;const auto before=rootCalls;Run();Check(rootCalls==before+1&&!Get(0,out),"full nonzero root argument forwarded unchanged but not qualified");
  Reset();const auto rootsBefore=rootCalls;Check(callRoot(f->roots[0],0)==rootResult&&rootCalls==rootsBefore+1,"root wrapper preserves full EAX and ret4 outside manager");Check(!Get(0,out),"outside-batch root update cannot publish");
  Reset();Run();Check(Get(0,w),"baseline before standalone producer");callRoot(f->roots[0],0);Check(!Get(0,out),"standalone world producer invalidates preceding phase");
}
static void FailuresAndThreads(){
  RejectMode(Mode::ForeignCaller,"foreign root caller cannot claim manager root dispatch");
  RejectMode(Mode::Frame,"frame change during original root rejects");
  RejectMode(Mode::Mutation,"retirement epoch change during original root rejects");
  RejectMode(Mode::Device,"device epoch change during original root rejects");
  RejectMode(Mode::SceneChange,"currentScene change during original root rejects");
  RejectMode(Mode::RootChange,"system root replacement during original root rejects");
  RejectMode(Mode::SystemChange,"partition root replacement during original root rejects");
  RejectMode(Mode::ManagerNotCleared,"manager normal return without currentScene clear cannot publish");
  source::Witness w{};
  Reset();Run();Check(Get(0,w),"baseline before later failing update");mode=Mode::ManagerException;
  Check(CatchManager(),"original manager SEH propagates unchanged");Check(!Get(0,w)&&source::active==nullptr&&source::Running()==0,"later same-frame failure invalidates previous success and restores TLS");
  Reset();mode=Mode::RootException;const auto mc=managerCalls,rc=rootCalls;
  Check(CatchManager(),"original root SEH propagates unchanged");Check(managerCalls==mc+1&&rootCalls==rc+1,"SEH does not replay originals");
  Check(Word(f->manager,0x20)==Ptr(f->scenes[0]),"observer does not repair native currentScene after SEH");
  Check(!Get(0,w)&&!source::active&&source::Running()==0&&source::rootDepth==0,"root SEH restores both adapter scopes");
  Reset();mode=Mode::ManagerCatchesRootException;Run();Check(!Get(0,w)&&source::Running()==0,"caught root failure cannot publish from normal manager return");
  Reset();mode=Mode::Reenter;const auto reentryCalls=managerCalls;Run();Check(managerCalls==reentryCalls+2&&!Get(0,w)&&!source::active&&source::Running()==0,"nested manager originals execute once each; both batches invalidated and parent restored");
  Reset();mode=Mode::Descendant;const auto children=rootCalls;Run();Check(rootCalls==children+2&&Get(0,w)&&source::tokenCount==1,"descendant root call cannot create duplicate manager witness");
  Reset();Run();Check(Get(0,w),"baseline before foreign manager");
  auto t=CreateThread(nullptr,0,ForeignManager,nullptr,0,nullptr);Check(t&&WaitForSingleObject(t,2000)==WAIT_OBJECT_0,"foreign manager thread completed");CloseHandle(t);
  Check(!Get(0,w)&&source::Running()==0&&!source::active,"foreign-thread manager atomically invalidates owner tokens");
  Reset();mode=Mode::ForeignConcurrent;const auto concurrentCalls=managerCalls;Run();Check(managerCalls==concurrentCalls+2&&!Get(0,w)&&source::Running()==0,"concurrent foreign update invalidates in-progress owner batch");
  Reset();mode=Mode::ForeignRootBlocked;foreignEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);foreignRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  Check(foreignEntered&&foreignRelease,"owned standalone-root synchronization events");
  t=CreateThread(nullptr,0,ForeignRoot,nullptr,0,nullptr);Check(t&&WaitForSingleObject(foreignEntered,2000)==WAIT_OBJECT_0,"foreign standalone root entered original and remains blocked");
  Run();Check(!Get(0,w)&&source::Running()==1,"owner manager cannot publish across an in-flight foreign standalone root");
  SetEvent(foreignRelease);Check(WaitForSingleObject(t,2000)==WAIT_OBJECT_0,"foreign standalone root exits");CloseHandle(t);CloseHandle(foreignEntered);CloseHandle(foreignRelease);
  Check(!Get(0,w)&&source::Running()==0,"overlapping root completion cannot revive invalid owner witness");
  Reset();Run();Check(Get(0,w),"fresh normal update recovers eligibility after exceptional/reentrant/foreign batches");
}
static void CapacityAndTransparency(){
  source::Witness w{};Reset();sceneCount=64;const auto r=rootCalls;Run();Check(rootCalls==r+64&&source::tokenCount==64&&Get(0,w)&&Get(63,w),"bounded table admits 64 actual distinct scene completions");
  Reset();sceneCount=65;const auto over=source::overflowed;const auto rr=rootCalls;Run();Check(rootCalls==rr+65&&source::overflowed==over+1&&source::tokenCount==0&&!Get(0,w),"65th root rejects batch without skipping native updates");
  Reset();auto original=std::make_unique<Fixture>(*f);
  source::enabled=false;const auto disabledResult=Run();auto nativeOnly=std::make_unique<Fixture>(*f);
  *f=*original;source::enabled=true;const auto observedResult=Run();
  Check(disabledResult==observedResult&&!memcmp(nativeOnly.get(),f,sizeof(Fixture)),"enabled and disabled observer leave byte-identical native producer state and result");
  Check(Get(0,w),"transparent observed run still publishes");
}
}
int main(){
  using namespace update_test;
  static_assert(sizeof(void*)==4,"x86 ABI fixture required");
  static_assert(std::is_trivial_v<source::Witness>&&std::is_trivially_copyable_v<source::Batch>,"fixed POD observer storage");
  try{
    Watchdog watchdog;auto fixture=std::make_unique<Fixture>();f=fixture.get();f->Reset();OwnedCode code;
    Install(code);NormalAndGuards(code);FailuresAndThreads();CapacityAndTransparency();source::EndFrame();
    if(source::output){fclose(source::output);source::output=nullptr;}
    printf("{\"status\":\"PASS\",\"checks\":%u,\"managerCalls\":%ld,\"rootCalls\":%ld,\"nativeGameCodeExecuted\":false,\"gpu\":false,\"ownedX86Thunks\":true}\n",checks,managerCalls,rootCalls);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL after %u checks: %s\n",checks,error.what());return 1;}
}
