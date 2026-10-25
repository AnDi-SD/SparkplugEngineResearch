// Own bounded CPU fixture for the exact server audit header. No D3D/API/GPU.
#define REMIX_BRIDGE_INSTANCE_AUDIT_TEST
#include "instance_audit.h"
#include <cstdlib>
using bridge_instance_audit::Audit;
static unsigned checks;
static void Check(bool value,const char* text){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
struct Watchdog {
  HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr),thread=nullptr;
  static DWORD WINAPI Wait(void* event){if(WaitForSingleObject(event,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe052ab30u);return 0;}
  Watchdog(){Check(event!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
  ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
static void Path(wchar_t* out,size_t size,const wchar_t* directory,const wchar_t* name) {
  Check(swprintf_s(out,size,L"%s\\%s",directory,name)>0,"owned absolute output path");
}
int wmain(int argc,wchar_t** argv) {
  Watchdog watchdog;Check(argc==2,"owned fresh directory argument");
  wchar_t path[4096]{};Path(path,4096,argv[1],L"sequence.jsonl");
  Audit disabled;Check(!disabled.Enabled()&&!disabled.Open(L"relative.jsonl")&&!disabled.Open(L"C:"),"unset/relative/truncated paths reject without writes");
  disabled.Command(1);disabled.Instance(0,true);Check(!disabled.Sequence()&&!disabled.Totals().calls,"disabled audit has no hot-path work");
  Audit a;Check(a.Open(path),"fresh full path opens");
  a.Command(100);a.Registered(17,0x1000,0);a.LinkImplicit(0x1000,31,0x2000);
  Check(a.Epoch()==1&&a.PresentIndex()==0,"registration establishes first device epoch");
  a.Command(101);a.Instance(0,true);a.Command(102);a.Instance(3,false);
  Check(a.Current().api.calls==2&&a.Current().api.earlyCalls==2&&a.Current().api.earlySuccess==1&&a.Current().api.earlyErrors==1&&a.Current().api.invalidMeshes==1,
    "real return classification, invalid handle and early counts are independent");
  a.Command(103);a.BeforeDraw(0x1000,1,0);
  Check(a.Current().draws==1&&a.Current().zeroDraws==1&&a.Current().firstDrawUid==103&&a.Current().firstDrawCount==0,
    "first zero-count ordinary command closes the early window");
  a.Command(104);a.Instance(0,true);
  Check(a.Current().api.calls==3&&a.Current().api.earlyCalls==2&&a.Current().api.success==2&&a.Current().firstError==3,
    "later API calls retain result counts but are not early");
  a.Command(105);a.PresentSwap(0x2000,0);
  Check(a.PresentIndex()==1&&!a.Current().draws&&!a.Current().api.calls&&a.Totals().calls==3,"implicit swapchain Present emits and clears exactly one interval");
  for(uint32_t kind=1;kind<=4;++kind){a.Command(110+kind);a.BeforeDraw(0x1000,kind,kind);a.Instance(0,true);}
  Check(a.Current().draws==4&&!a.Current().api.earlyCalls,"all four ordinary draw kinds close early classification");
  a.Command(120);a.PresentDevice(0x1000,static_cast<int32_t>(0x80004005u));
  Check(a.PresentIndex()==2&&a.Totals().calls==7,"failed Device Present still closes an explicitly failed interval");
  a.Command(120);Check(a.Sequence()==12,"queue sequence remains monotonic despite a repeated UID");
  a.BeforeDraw(0x9000,2,10);a.Instance(0,true);a.PresentSwap(0x9000,0);
  Check(!a.Current().draws&&a.Current().foreignDraws==1&&a.Current().foreignPresents==1&&!a.Current().orderKnown,
    "foreign device/swapchain cannot close target interval or silently qualify order");
  a.LinkImplicit(0x9000,41,0x9000);a.Command(121);a.PresentSwap(0x2000,0);
  Check(a.PresentIndex()==3,"foreign Link does not replace registered implicit swapchain");
  a.Command(130);a.Instance(0,true);const auto resetEpoch=a.Epoch();a.ResetBegin(0x1000);
  Check(a.Epoch()==resetEpoch+1&&!a.Current().api.calls&&a.PresentIndex()==3,"Reset closes incomplete interval and advances epoch without inventing Present");
  a.ResetResult(0x1000,static_cast<int32_t>(0x80004005u));Check(!a.Current().orderKnown,"failed Reset disqualifies subsequent interval");
  a.Command(131);a.Instance(0,true);a.Command(132);a.PresentSwap(0x2000,0);
  a.Command(133);a.ResetBegin(0x1000);a.ResetResult(0x1000,0);Check(a.Current().orderKnown,"successful reset begins a fresh order interval");
  a.Command(134);a.Instance(0,true);a.PresentDevice(0x1000,0);
  const auto beforeDestroyEpoch=a.Epoch();a.Command(140);a.Instance(0,true);a.DestroySwap(0x2000);
  Check(a.Epoch()==beforeDestroyEpoch+1&&!a.Current().api.calls,"implicit swapchain destroy closes incomplete interval");
  a.PresentSwap(0x2000,0);Check(a.Current().foreignPresents==1,"destroyed swapchain identity cannot close a frame");
  a.LinkImplicit(0x1000,32,0x2001);a.Command(141);a.PresentSwap(0x2001,0);
  a.Command(150);a.Instance(0,true);a.DestroyDevice(0x1000);
  Check(!a.Current().api.calls,"device destruction closes its unfinished API work");
  a.Command(151);a.Instance(7,true);Check(!a.Current().orderKnown,"API before a registered device is explicitly unqualified");
  a.Command(152);a.Registered(18,0x1001,7);a.LinkImplicit(0x1001,33,0x2002);a.Instance(7,true);a.PresentSwap(0x2002,0);
  a.Command(153);a.Registered(19,0x1002,0);a.LinkImplicit(0x1002,34,0x2003);
  a.Command(154);a.Instance(0,true);const auto total=a.Totals();a.Command(155);a.QueueExit(false);
  Check(!a.Enabled()&&total.calls==16&&total.success==13&&total.errors==3&&total.invalidMeshes==1,
    "queue exit preserves exact cumulative API returns and incomplete tail");
  Check(a.Totals().calls==a.Totals().success+a.Totals().errors&&a.Totals().earlyCalls==a.Totals().earlySuccess+a.Totals().earlyErrors,
    "cumulative counts satisfy success/error conservation");
  Audit duplicate;Check(!duplicate.Open(path)&&duplicate.IoError()==ERROR_FILE_EXISTS,"duplicate path never overwrites prior evidence");
  Path(path,4096,argv[1],L"cap.jsonl");Audit cap;Check(cap.Open(path),"cap fixture fresh path");cap.SetByteLimitForTest(2048);cap.Registered(1,1,0);
  for(unsigned i=0;i<8&&cap.Enabled();++i){cap.Command(i);cap.Instance(0,true);cap.PresentDevice(1,0);}
  Check(cap.Capped()&&!cap.Enabled()&&cap.BytesWritten()<=2048,"size cap emits terminal marker and disables only audit");
  Path(path,4096,argv[1],L"io-failure.jsonl");Audit io;Check(io.Open(path),"I/O failure fixture path");io.Registered(1,1,0);io.Command(1);io.Instance(0,true);
  io.FailNextWriteForTest();io.PresentDevice(1,0);const auto calls=io.Totals().calls;io.Command(2);io.Instance(0,true);
  Check(!io.Enabled()&&io.IoError()==ERROR_WRITE_FAULT&&io.Totals().calls==calls,"I/O failure disables audit and leaves no later file/queue work");
  Path(path,4096,argv[1],L"missing-parent\\never-created.jsonl");Audit missing;Check(!missing.Open(path)&&missing.IoError()==ERROR_PATH_NOT_FOUND,"absent parent disables audit without creating directories");
  Path(path,4096,argv[1],L"normal-exit.jsonl");Audit normal;Check(normal.Open(path),"normal exit fixture path");normal.Command(1);normal.Registered(1,1,0);normal.QueueExit(true);
  Check(!normal.Enabled(),"normal queue exit closes audit handle");
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"apiCalls\":%llu,\"success\":%llu,\"errors\":%llu,\"gpu\":false,\"nativeCode\":false}\n",
    checks,total.calls,total.success,total.errors);
}
