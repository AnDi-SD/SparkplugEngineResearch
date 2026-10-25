// Own Windows-only process exit oracle. No graphics, game or engine code.
#include <windows.h>
#ifdef OWN_DLL
namespace { HANDLE entered=nullptr,releaseGate=nullptr; }
extern "C" __declspec(dllexport) void SetExitGate(HANDLE first,HANDLE second){entered=first;releaseGate=second;}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID reserved){
  if(reason==DLL_PROCESS_DETACH&&reserved&&entered&&releaseGate){
    SetEvent(entered);
    WaitForSingleObject(releaseGate,INFINITE); // Deliberate own failure state, bounded by parent.
  }
  return TRUE;
}
#else
int wmain(int argc,wchar_t** argv){
  if(argc!=3)return 90;
  HANDLE entered=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[1]);
  HANDLE releaseGate=OpenEventW(SYNCHRONIZE,FALSE,argv[2]);
  if(!entered||!releaseGate)return 91;
  HMODULE module=LoadLibraryW(L"detach_gate.dll");if(!module)return 92;
  using Configure=void(*)(HANDLE,HANDLE);
  auto configure=reinterpret_cast<Configure>(GetProcAddress(module,"SetExitGate"));
  if(!configure)return 93;configure(entered,releaseGate);
  ExitProcess(7);
}
#endif
