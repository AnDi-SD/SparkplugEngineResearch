// Own ownership oracle for the extracted bridge release method.
// Every process and handle used here belongs to this fixture.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "shutdown_methods.h"
static FILE* journal;
static void Require(bool ok,const char* message){
  if(!ok){fprintf(journal,"{\"error\":\"%s\",\"win32\":%lu}\n",message,GetLastError());fflush(journal);std::exit(1);}
}
template<class Method> static void Case(unsigned index,const char* profile,bool expectedMarkerOpen){
  DWORD handlesBefore=0;Require(GetProcessHandleCount(GetCurrentProcess(),&handlesBefore)!=FALSE,"initial handle count");
  std::vector<HANDLE> markers;
  for(unsigned n=0;n<256;++n){HANDLE h=CreateEventA(nullptr,TRUE,FALSE,nullptr);Require(h!=nullptr,"own marker event");markers.push_back(h);}
  char executable[MAX_PATH]{};Require(GetModuleFileNameA(nullptr,executable,MAX_PATH)!=0,"own executable");
  const auto prefix="Local\\WinxRemixRemoteHandle-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(index);
  const auto readyName=prefix+"-ready",releaseName=prefix+"-release";
  HANDLE ready=CreateEventA(nullptr,TRUE,FALSE,readyName.c_str());Require(ready&&GetLastError()!=ERROR_ALREADY_EXISTS,"fresh ready event");
  HANDLE release=CreateEventA(nullptr,TRUE,FALSE,releaseName.c_str());Require(release&&GetLastError()!=ERROR_ALREADY_EXISTS,"fresh release event");
  auto command="\""+std::string(executable)+"\" --child "+readyName+" "+releaseName;
  STARTUPINFOA startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
  Require(CreateProcessA(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"own child create");
  CloseHandle(child.hThread);
  fprintf(journal,"{\"event\":\"child\",\"index\":%u,\"pid\":%lu,\"profile\":\"%s\"}\n",index,child.dwProcessId,profile);fflush(journal);
  HANDLE childObserver=nullptr;Require(DuplicateHandle(GetCurrentProcess(),child.hProcess,GetCurrentProcess(),&childObserver,0,FALSE,DUPLICATE_SAME_ACCESS)!=FALSE,"retained own child observer");
  Require(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0,"child readiness");
  // Allocate handles only in our child until one has the same numeric value
  // as an event we own in the parent. Its object identity remains different.
  HANDLE remote=nullptr,marker=nullptr;unsigned remoteAllocations=0;
  for(;remoteAllocations<4096&&!marker;++remoteAllocations){
    Require(DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),child.hProcess,&remote,0,FALSE,DUPLICATE_SAME_ACCESS)!=FALSE,"remote parent process handle");
    for(HANDLE event:markers)if(event==remote){marker=event;break;}
  }
  Require(marker!=nullptr,"bounded own-table numeric collision");
  HANDLE remoteObserver=nullptr;
  Require(DuplicateHandle(child.hProcess,remote,GetCurrentProcess(),&remoteObserver,0,FALSE,DUPLICATE_SAME_ACCESS)!=FALSE,"observe remote object identity");
  Require(GetProcessId(remoteObserver)==GetCurrentProcessId(),"remote handle identifies parent process");
  Require(CloseHandle(remoteObserver)!=FALSE,"close remote observer in parent");
  DWORD flags=0;Require(GetHandleInformation(marker,&flags)!=FALSE,"parent marker initially valid");
  Require(SetEvent(release)!=FALSE,"release own child");
  Method method;method.hProcess=child.hProcess;method.hDuplicate=remote;
  const auto begin=GetTickCount64();method.releaseChildProcess();const auto elapsed=GetTickCount64()-begin;
  Require(WaitForSingleObject(childObserver,5000)==WAIT_OBJECT_0,"child actually exited");
  DWORD exitCode=STILL_ACTIVE;Require(GetExitCodeProcess(childObserver,&exitCode)!=FALSE,"child exit code");
  SetLastError(ERROR_SUCCESS);const bool markerOpen=GetHandleInformation(marker,&flags)!=FALSE;const DWORD markerError=GetLastError();
  Require(markerOpen==expectedMarkerOpen,"parent marker ownership outcome");
  if(markerOpen)Require(SetEvent(marker)!=FALSE,"preserved marker remains usable");
  else Require(markerError==ERROR_INVALID_HANDLE,"closed marker is invalid handle");
  SetLastError(ERROR_SUCCESS);Require(!GetHandleInformation(child.hProcess,&flags)&&GetLastError()==ERROR_INVALID_HANDLE,"original child handle closed");
  Require(exitCode==0&&elapsed<=2000,"bounded normal child exit");
  for(HANDLE event:markers)if(event!=marker||markerOpen)Require(CloseHandle(event)!=FALSE,"marker cleanup");
  Require(CloseHandle(childObserver)&&CloseHandle(ready)&&CloseHandle(release),"remaining own handle cleanup");
  DWORD handlesAfter=0;Require(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter)!=FALSE,"final handle count");
  fprintf(journal,"{\"event\":\"handle_balance\",\"index\":%u,\"before\":%lu,\"after\":%lu,\"parentMarkerOpen\":%s,\"childExitCode\":%lu}\n",index,handlesBefore,handlesAfter,markerOpen?"true":"false",exitCode);fflush(journal);
  Require(handlesBefore==handlesAfter,"no retained parent handles");
  fprintf(journal,"{\"event\":\"case\",\"index\":%u,\"profile\":\"%s\",\"childExitCode\":%lu,\"remoteHandleValue\":%llu,\"remoteAllocations\":%u,\"remoteObjectIsParent\":true,\"parentMarkerOpen\":%s,\"expectedMarkerOpen\":%s,\"milliseconds\":%llu,\"handlesBefore\":%lu,\"handlesAfter\":%lu,\"passed\":true}\n",index,profile,exitCode,(unsigned long long)(uintptr_t)remote,remoteAllocations,markerOpen?"true":"false",expectedMarkerOpen?"true":"false",elapsed,handlesBefore,handlesAfter);fflush(journal);
}
int main(int argc,char** argv){
  if(argc==2&&std::string(argv[1])=="--warm-child")return 0;
  if(argc==4&&std::string(argv[1])=="--child"){
    HANDLE ready=OpenEventA(EVENT_MODIFY_STATE,FALSE,argv[2]),release=OpenEventA(SYNCHRONIZE,FALSE,argv[3]);
    if(!ready||!release)return 90;
    if(!SetEvent(ready))return 91;
    const DWORD result=WaitForSingleObject(release,15000);CloseHandle(ready);CloseHandle(release);Sleep(200);
    return result==WAIT_OBJECT_0?0:92;
  }
  if(argc!=1)return 93;
  if(fopen_s(&journal,"fixture.jsonl","wb")||!journal)return 94;
  // Measure the OS/CRT's first process-creation initialization separately.
  DWORD coldCount=0,warmCount=0;Require(GetProcessHandleCount(GetCurrentProcess(),&coldCount)!=FALSE,"cold count");
  char executable[MAX_PATH]{};Require(GetModuleFileNameA(nullptr,executable,MAX_PATH)!=0,"warm image");
  auto command="\""+std::string(executable)+"\" --warm-child";
  STARTUPINFOA startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
  Require(CreateProcessA(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"own warm child");
  Require(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0,"warm child exit");
  DWORD exitCode=STILL_ACTIVE;Require(GetExitCodeProcess(child.hProcess,&exitCode)&&exitCode==0,"warm child code");
  Require(CloseHandle(child.hProcess)&&CloseHandle(child.hThread),"warm handle cleanup");
  Require(GetProcessHandleCount(GetCurrentProcess(),&warmCount)!=FALSE,"warm count");
  fprintf(journal,"{\"event\":\"warmup\",\"pid\":%lu,\"exitCode\":%lu,\"coldHandles\":%lu,\"warmHandles\":%lu}\n",child.dwProcessId,exitCode,coldCount,warmCount);fflush(journal);
  Case<baseline::Process>(0,"baseline",false);
  Case<candidate::Process>(1,"candidate",true);
  fputs("{\"event\":\"complete\",\"cases\":2}\n",journal);fclose(journal);return 0;
}
