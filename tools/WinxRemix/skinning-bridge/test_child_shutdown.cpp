// Own process-only oracle for the extracted upstream releaseChildProcess body.
// No renderer, graphics device, game assets or unrelated process is opened.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "shutdown_methods.h"
static FILE* journal;
static void Require(bool value,const char* message){if(!value){fprintf(journal,"{\"error\":\"%s\",\"win32\":%lu}\n",message,GetLastError());fflush(journal);std::exit(1);}}
template<class Method>
static void Case(unsigned index,const char* profile,const char* name,DWORD delay,DWORD requestedExit,DWORD expectedExit,ULONGLONG minimumMs,ULONGLONG maximumMs){
  char executable[MAX_PATH]{};Require(GetModuleFileNameA(nullptr,executable,MAX_PATH)!=0,"own image path");
  const std::string eventName="Local\\WinxRemixShutdown-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(index);
  HANDLE ready=CreateEventA(nullptr,TRUE,FALSE,eventName.c_str());Require(ready&&GetLastError()!=ERROR_ALREADY_EXISTS,"fresh child event");
  std::string command="\""+std::string(executable)+"\" --child "+eventName+" "+std::to_string(delay)+" "+std::to_string(requestedExit);
  STARTUPINFOA startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
  Require(CreateProcessA(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"own child create");
  CloseHandle(child.hThread);
  HANDLE observer=nullptr;Require(DuplicateHandle(GetCurrentProcess(),child.hProcess,GetCurrentProcess(),&observer,0,FALSE,DUPLICATE_SAME_ACCESS)!=FALSE,"own observer handle");
  fprintf(journal,"{\"event\":\"child\",\"index\":%u,\"pid\":%lu,\"profile\":\"%s\",\"case\":\"%s\"}\n",index,child.dwProcessId,profile,name);fflush(journal);
  Require(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0,"child readiness");
  Method method;method.hProcess=child.hProcess;
  const auto begin=GetTickCount64();method.releaseChildProcess();const auto elapsed=GetTickCount64()-begin;
  Require(WaitForSingleObject(observer,5000)==WAIT_OBJECT_0,"child actually exited");
  DWORD exitCode=STILL_ACTIVE;Require(GetExitCodeProcess(observer,&exitCode)!=FALSE,"child exit status");
  DWORD flags=0;SetLastError(ERROR_SUCCESS);
  const bool originalHandleClosed=!GetHandleInformation(child.hProcess,&flags)&&GetLastError()==ERROR_INVALID_HANDLE;
  Require(CloseHandle(observer)!=FALSE&&CloseHandle(ready)!=FALSE,"own observer cleanup");
  const bool passed=exitCode==expectedExit&&elapsed>=minimumMs&&elapsed<=maximumMs&&originalHandleClosed;
  fprintf(journal,"{\"event\":\"case\",\"index\":%u,\"profile\":\"%s\",\"case\":\"%s\",\"delay\":%lu,\"requestedExit\":%lu,\"exitCode\":%lu,\"expectedExit\":%lu,\"milliseconds\":%llu,\"handleClosed\":%s,\"passed\":%s}\n",index,profile,name,delay,requestedExit,exitCode,expectedExit,elapsed,originalHandleClosed?"true":"false",passed?"true":"false");fflush(journal);
  Require(passed,"shutdown case outcome");
}
int main(int argc,char** argv){
  if(argc==5&&std::string(argv[1])=="--child"){
    HANDLE ready=OpenEventA(EVENT_MODIFY_STATE,FALSE,argv[2]);if(!ready)return 90;
    if(!SetEvent(ready))return 91;CloseHandle(ready);Sleep(DWORD(std::strtoul(argv[3],nullptr,10)));return int(std::strtoul(argv[4],nullptr,10));
  }
  if(argc!=1)return 92;
  if(fopen_s(&journal,"fixture.jsonl","wb")||!journal)return 93;
  Case<baseline::Process>(0,"baseline","fast",100,0,0,0,2000);
  Case<baseline::Process>(1,"baseline","slow-clean",4000,0,1,2750,4500);
  Case<baseline::Process>(2,"baseline","reported-error",100,23,23,0,2000);
  Case<baseline::Process>(3,"baseline","hung",15000,0,1,2750,4500);
  Case<candidate::Process>(4,"candidate","fast",100,0,0,0,2000);
  Case<candidate::Process>(5,"candidate","slow-clean",4000,0,0,3500,6500);
  Case<candidate::Process>(6,"candidate","reported-error",100,23,23,0,2000);
  Case<candidate::Process>(7,"candidate","hung",15000,0,1,9750,12500);
  fputs("{\"event\":\"complete\",\"cases\":8}\n",journal);fclose(journal);return 0;
}
