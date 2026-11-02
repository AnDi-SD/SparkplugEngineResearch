/* Test-only XP/x86 original-game/Direct3D observer. No CRT, input injection,
   credentials, VM API, driver installation or production WinxCoop code. */
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stddef.h>
#include <string.h>
#if !defined(_M_IX86)
#error XpGameProbe requires the x86 compiler.
#endif

#pragma function(memcpy, memset)
void* __cdecl memcpy(void* output,const void* input,size_t size)
{ volatile unsigned char* to=(volatile unsigned char*)output; const volatile unsigned char* from=(const volatile unsigned char*)input; size_t i; for(i=0;i<size;++i)to[i]=from[i];return output; }
void* __cdecl memset(void* output,int value,size_t size)
{ volatile unsigned char* to=(volatile unsigned char*)output;size_t i;for(i=0;i<size;++i)to[i]=(unsigned char)value;return output; }

static char Game[MAX_PATH],Media[MAX_PATH],Report[MAX_PATH],Directory[MAX_PATH];
static char Token[512],SidecarPath[MAX_PATH],Sidecar[1025],Command[MAX_PATH+3];
static char D3DPath[MAX_PATH],LoadedD3DPath[MAX_PATH],Json[8192];
static BYTE Previous[8192],Verified[8192];
static DWORD JsonSize,Began,FileError,ArgumentError,D3DError,ModulePathError;
static DWORD ParentDisposition,GameDisposition,RegistryOpenError,SnapshotError,OverrideError;
static DWORD RestoreError,VerifyError,GameDeleteError,ParentDeleteError,RegistryCloseError;
static DWORD PreviousType,PreviousSize,LanguageStatus,LanguageType,LanguageValue,LanguageBytes;
static DWORD LaunchError,WaitResult=WAIT_FAILED,WaitError,ExitCode=STILL_ACTIVE,ExitQueryError;
static DWORD TerminateError,TerminationWait=WAIT_FAILED,ProcessId,ObserveMs;
static DWORD CloseProcessError,CloseThreadError,FreeLibraryError,ReleasedRefs;
static DWORD JobCreateError,JobConfigureError,JobAssignError,JobTerminateError,JobQueryError,JobCloseError;
static DWORD JobQueryCalls,JobInitialActive,JobLastActive,JobTotalAssigned,JobCleanupMs,ResumeResult=0xffffffffUL,ResumeError;
static DWORD ObservationLimitMs=40000;
static int HaveGame,HaveMedia,HaveReport,SidecarUsed,VersionOK,SnapshotOK,PreviousExists;
static int HaveObserve;
static int OverrideAttempted,OverrideOK,RestoreAttempted,RestoreVerified,CreatedProcess,TimedOut,Terminated,ChildStopped;
static int JobCreated,JobConfigured,JobAssigned,JobTerminateAttempted,JobTerminated,JobActiveZero,JobClosed;
static int JobInitialObserved,JobLastQuerySucceeded;
static int ResumeAttempted,ChildResumed,ProcessCleanupAttempted,ProcessTreeStopped,RestoreBlockedByProcessTree;
static int IdentifierCalled,DisplayCalled,HalCapsCalled,RefCapsCalled,HalCheckCalled,RefCheckCalled,LanguageQueried;
static HRESULT IdentifierHR=E_FAIL,DisplayHR=E_FAIL,HalCapsHR=E_FAIL,RefCapsHR=E_FAIL,HalCheckHR=E_FAIL,RefCheckHR=E_FAIL;
static OSVERSIONINFOEXA Version;
static D3DADAPTER_IDENTIFIER9 Identifier;
static D3DDISPLAYMODE Mode;
static D3DCAPS9 HalCaps,RefCaps;
static HMODULE D3DModule;
static IDirect3D9* D3D;
static HKEY ParentKey,GameKey;
static HANDLE ReportFile=INVALID_HANDLE_VALUE;
static HANDLE ChildJob;
static HANDLE MediaLease;
static DWORD MediaLeaseCreateError,MediaLeaseCloseError;
static int MediaLeaseOwned,MediaLeaseClosed,OwnMediaLeaseRequested;
static char NextMedia[MAX_PATH],BrokerBase[MAX_PATH],BrokerState[MAX_PATH],BrokerRequest[MAX_PATH],BrokerAck[MAX_PATH];
static char BrokerRequestPending[MAX_PATH],BrokerAckPending[MAX_PATH],BrokerActual[MAX_PATH];
static char BrokerParentPid[11],BrokerChildPid[11],BrokerParentTime[17],BrokerChildTime[17];
static char BrokerFrame[2048],BrokerInput[512];
static DWORD BrokerFrameBytes,BrokerError,BrokerReadError,BrokerSetError,BrokerAckError,BrokerCloseError,BrokerPolls;
static DWORD BrokerActualType,BrokerActualBytes,BrokerRequestBytes;
static int HaveNextMedia,BrokerPathsReady,BrokerStateWritten,BrokerRequestSeen,BrokerRequestValid;
static int BrokerHostReadback,BrokerHostBeforeSwitchVerified,BrokerSwitchAttempted,BrokerSwitchSucceeded,BrokerGuestReadback,BrokerAckWritten;
static PROCESS_INFORMATION Child;
static STARTUPINFOA Startup;
static const char* Failure="none";
typedef IDirect3D9* (WINAPI *Create9Fn)(UINT);

static DWORD Length(const char* value)
{ DWORD n=0;while(value[n])++n;return n; }
static int Equal(const char* a,const char* b)
{ while(*a && *a==*b){++a;++b;}return *a==*b; }
static int EqualBytes(const BYTE* a,const BYTE* b,DWORD size)
{ DWORD i;for(i=0;i<size;++i)if(a[i]!=b[i])return 0;return 1; }
static int Fail(const char* text)
{ if(Equal(Failure,"none"))Failure=text;return 0; }
static int Space(char c)
{ return c==' '||c=='\t'||c=='\r'||c=='\n'; }
static int Next(const char** input,char* output,DWORD capacity)
{
    const char* p=*input;DWORD n=0;int quote;
    while(Space(*p))++p;if(!*p){*input=p;return 0;}
    quote=*p=='"';if(quote)++p;
    while(*p && (quote?*p!='"':!Space(*p)))
    { unsigned char c=(unsigned char)*p++;if(c<32||c>126||c=='"'||n+1>=capacity)return -1;output[n++]=(char)c; }
    if(quote){if(*p!='"')return -1;++p;if(*p&&!Space(*p))return -1;}
    output[n]=0;*input=p;return 1;
}
static int Absolute(const char* path)
{ return ((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))&&path[1]==':'&&path[2]=='\\'; }
static int BrokerFail(const char* stage,DWORD error)
{if(!BrokerError)BrokerError=error?error:ERROR_INVALID_DATA;return Fail(stage);}
static char UpperAscii(char c){return c>='a'&&c<='z'?(char)(c-'a'+'A'):c;}
static int PrefixInsensitive(const char* value,const char* prefix)
{while(*prefix){if(!*value||UpperAscii(*value++)!=UpperAscii(*prefix++))return 0;}return 1;}
static int BrokerNormalizeMedia(char* path)
{
    char full[MAX_PATH];DWORD n,attributes;
    if(!Absolute(path))return BrokerFail("media-broker-absolute-media",ERROR_INVALID_NAME);
    n=GetFullPathNameA(path,sizeof(full),full,0);
    if(!n||n+2>sizeof(full))return BrokerFail("media-broker-media-bound",n?ERROR_INSUFFICIENT_BUFFER:GetLastError());
    if(full[n-1]!='\\'){full[n++]='\\';full[n]=0;}
    attributes=GetFileAttributesA(full);
    if(attributes==INVALID_FILE_ATTRIBUTES)return BrokerFail("media-broker-media-directory",GetLastError());
    if(!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return BrokerFail("media-broker-media-directory",ERROR_INVALID_DATA);
    memcpy(path,full,n+1);return 1;
}
static int BrokerFileAbsent(const char* path)
{
    DWORD attributes,error;
    attributes=GetFileAttributesA(path);
    if(attributes!=INVALID_FILE_ATTRIBUTES)return BrokerFail("media-broker-sidecar-exists",ERROR_ALREADY_EXISTS);
    error=GetLastError();
    if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return BrokerFail("media-broker-sidecar-inspect",error);
    return 1;
}
static int BrokerPath(char* path,const char* suffix)
{
    DWORD base=Length(BrokerBase),extra=Length(suffix);
    if(base+extra+1>MAX_PATH)return BrokerFail("media-broker-sidecar-bound",ERROR_INSUFFICIENT_BUFFER);
    memcpy(path,BrokerBase,base);memcpy(path+base,suffix,extra+1);return BrokerFileAbsent(path);
}
static int BrokerPreflight(void)
{
    DWORD n;
    if(!HaveNextMedia)return 1;
    if(!OwnMediaLeaseRequested)return BrokerFail("media-broker-requires-owned-lease",ERROR_INVALID_PARAMETER);
    if(!BrokerNormalizeMedia(Media)||!BrokerNormalizeMedia(NextMedia))return 0;
    if(PrefixInsensitive(Media,NextMedia)||PrefixInsensitive(NextMedia,Media))return BrokerFail("media-broker-media-not-disjoint",ERROR_INVALID_PARAMETER);
    n=GetModuleFileNameA(0,BrokerBase,sizeof(BrokerBase));
    if(!n||n>=sizeof(BrokerBase))return BrokerFail("media-broker-executable-path",n?ERROR_INSUFFICIENT_BUFFER:GetLastError());
    if(!Absolute(BrokerBase))return BrokerFail("media-broker-executable-path",ERROR_INVALID_NAME);
    if(!BrokerPath(BrokerState,".media-state")||!BrokerPath(BrokerRequest,".media-request")||
        !BrokerPath(BrokerAck,".media-ack")||!BrokerPath(BrokerRequestPending,".media-request.pending")||
        !BrokerPath(BrokerAckPending,".media-ack.pending"))return 0;
    BrokerPathsReady=1;return 1;
}
static int ParseObservationLimit(const char* text)
{
    DWORD value=0,digit;
    if(!*text)return Fail("observe-ms-empty");
    while(*text)
    {
        if(*text<'0'||*text>'9')return Fail("observe-ms-decimal");
        digit=(DWORD)(*text++-'0');
        if(value>(0xffffffffUL-digit)/10)return Fail("observe-ms-overflow");
        value=value*10+digit;
    }
    if(value<1000||value>3700000)return Fail("observe-ms-range");
    ObservationLimitMs=value;return 1;
}
static int LoadSidecar(void)
{
    HANDLE h;DWORD n,high=0,size,read=0,i;BOOL ok;
    n=GetModuleFileNameA(0,SidecarPath,sizeof(SidecarPath));
    if(!n||n+5>=sizeof(SidecarPath))return Fail("sidecar-path");
    memcpy(SidecarPath+n,".args",6);
    h=CreateFileA(SidecarPath,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE){ArgumentError=GetLastError();return Fail("sidecar-open");}
    SetLastError(NO_ERROR);size=GetFileSize(h,&high);
    if(high||!size||size>=sizeof(Sidecar)||(size==INVALID_FILE_SIZE&&GetLastError()!=NO_ERROR))
    {CloseHandle(h);return Fail("sidecar-size");}
    ok=ReadFile(h,Sidecar,size,&read,0);if(!ok)ArgumentError=GetLastError();
    if(!CloseHandle(h)&&!ArgumentError)ArgumentError=GetLastError();
    if(!ok||read!=size||ArgumentError)return Fail("sidecar-read");
    for(i=0;i<read;++i)
        if(((unsigned char)Sidecar[i]<32&&!Space(Sidecar[i]))||(unsigned char)Sidecar[i]>126)return Fail("sidecar-ascii");
    Sidecar[read]=0;SidecarUsed=1;return 1;
}
static int Arguments(void)
{
    const char* p=GetCommandLineA();int next;unsigned count=0;DWORD i,last=0,n,attributes;
    if(Next(&p,Token,sizeof(Token))!=1)return Fail("command-line");next=Next(&p,Token,sizeof(Token));
    if(!next){if(!LoadSidecar())return 0;p=Sidecar;next=Next(&p,Token,sizeof(Token));}
    while(next==1)
    {
        if(++count>6)return Fail("argument-count");
        if(Equal(Token,"--game")){if(HaveGame||Next(&p,Game,sizeof(Game))!=1)return Fail("game-argument");HaveGame=1;}
        else if(Equal(Token,"--media")){if(HaveMedia||Next(&p,Media,sizeof(Media))!=1)return Fail("media-argument");HaveMedia=1;}
        else if(Equal(Token,"--report")){if(HaveReport||Next(&p,Report,sizeof(Report))!=1)return Fail("report-argument");HaveReport=1;}
        else if(Equal(Token,"--observe-ms")){if(HaveObserve||Next(&p,Token,sizeof(Token))!=1||!ParseObservationLimit(Token))return Fail("observe-ms-argument");HaveObserve=1;}
        else if(Equal(Token,"--own-media-lease")){if(OwnMediaLeaseRequested)return Fail("duplicate-own-media-lease");OwnMediaLeaseRequested=1;}
        else if(Equal(Token,"--next-media")){if(HaveNextMedia||Next(&p,NextMedia,sizeof(NextMedia))!=1)return Fail("next-media-argument");HaveNextMedia=1;}
        else return Fail("unknown-argument");next=Next(&p,Token,sizeof(Token));
    }
    if(next<0||!HaveGame||!HaveMedia||!HaveReport||!Absolute(Game)||!Absolute(Media)||!Absolute(Report))return Fail("absolute-ascii-arguments");
    n=Length(Media);if(n<3||n+2>sizeof(Media))return Fail("media-bound");
    if(Media[n-1]!='\\'){Media[n++]='\\';Media[n]=0;}
    attributes=GetFileAttributesA(Game);if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY)){ArgumentError=GetLastError();return Fail("game-file");}
    attributes=GetFileAttributesA(Media);if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)){ArgumentError=GetLastError();return Fail("media-directory");}
    n=Length(Game);for(i=0;i<n;++i)if(Game[i]=='\\')last=i;
    memcpy(Directory,Game,last+1);Directory[last+1]=0;
    Command[0]='"';memcpy(Command+1,Game,n);Command[n+1]='"';Command[n+2]=0;
    ReportFile=CreateFileA(Report,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
    if(ReportFile==INVALID_HANDLE_VALUE){FileError=GetLastError();return Fail("fresh-report-create");}
    return BrokerPreflight();
}
static void Graphics(void)
{
    DWORD n;Create9Fn create;
    n=GetSystemDirectoryA(D3DPath,sizeof(D3DPath));
    if(!n||n+11>=sizeof(D3DPath)){D3DError=GetLastError();return;}
    memcpy(D3DPath+n,"\\d3d9.dll",10);
    D3DModule=LoadLibraryA(D3DPath);if(!D3DModule){D3DError=GetLastError();return;}
    n=GetModuleFileNameA(D3DModule,LoadedD3DPath,sizeof(LoadedD3DPath));
    if(!n||n>=sizeof(LoadedD3DPath)){ModulePathError=GetLastError();LoadedD3DPath[0]=0;}
    create=(Create9Fn)GetProcAddress(D3DModule,"Direct3DCreate9");
    if(!create){D3DError=GetLastError();return;}
    D3D=create(D3D_SDK_VERSION);if(!D3D)return;
    IdentifierCalled=1;IdentifierHR=IDirect3D9_GetAdapterIdentifier(D3D,D3DADAPTER_DEFAULT,0,&Identifier);
    Identifier.Driver[sizeof(Identifier.Driver)-1]=0;Identifier.Description[sizeof(Identifier.Description)-1]=0;
    DisplayCalled=1;DisplayHR=IDirect3D9_GetAdapterDisplayMode(D3D,D3DADAPTER_DEFAULT,&Mode);
    HalCapsCalled=1;HalCapsHR=IDirect3D9_GetDeviceCaps(D3D,D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&HalCaps);
    RefCapsCalled=1;RefCapsHR=IDirect3D9_GetDeviceCaps(D3D,D3DADAPTER_DEFAULT,D3DDEVTYPE_REF,&RefCaps);
    if(SUCCEEDED(DisplayHR))
    {
        HalCheckCalled=1;HalCheckHR=IDirect3D9_CheckDeviceType(D3D,0,D3DDEVTYPE_HAL,Mode.Format,Mode.Format,TRUE);
        RefCheckCalled=1;RefCheckHR=IDirect3D9_CheckDeviceType(D3D,0,D3DDEVTYPE_REF,Mode.Format,Mode.Format,TRUE);
    }
}
static int PrepareRegistry(void)
{
    LONG result;DWORD size;
    result=RegCreateKeyExA(HKEY_LOCAL_MACHINE,"Software\\Konami",0,0,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,0,&ParentKey,&ParentDisposition);
    if(result!=ERROR_SUCCESS){RegistryOpenError=(DWORD)result;return Fail("registry-parent-open");}
    result=RegCreateKeyExA(ParentKey,"Winx Club",0,0,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,0,&GameKey,&GameDisposition);
    if(result!=ERROR_SUCCESS){RegistryOpenError=(DWORD)result;return Fail("registry-game-open");}
    LanguageQueried=1;LanguageBytes=sizeof(LanguageValue);LanguageStatus=(DWORD)RegQueryValueExA(GameKey,"LanguageID",0,&LanguageType,(BYTE*)&LanguageValue,&LanguageBytes);
    result=RegQueryValueExA(GameKey,"MediaPath",0,&PreviousType,0,&PreviousSize);
    if(result==ERROR_FILE_NOT_FOUND){PreviousExists=0;PreviousSize=0;SnapshotOK=1;}
    else if(result!=ERROR_SUCCESS){SnapshotError=(DWORD)result;return Fail("registry-snapshot-size");}
    else
    {
        PreviousExists=1;if(PreviousSize>sizeof(Previous)){SnapshotError=ERROR_MORE_DATA;return Fail("registry-snapshot-bound");}
        size=sizeof(Previous);result=RegQueryValueExA(GameKey,"MediaPath",0,&PreviousType,Previous,&size);
        if(result!=ERROR_SUCCESS){SnapshotError=(DWORD)result;return Fail("registry-snapshot-read");}
        PreviousSize=size;SnapshotOK=1;
    }
    OverrideAttempted=1;result=RegSetValueExA(GameKey,"MediaPath",0,REG_SZ,(const BYTE*)Media,Length(Media)+1);
    if(result!=ERROR_SUCCESS){OverrideError=(DWORD)result;return Fail("registry-media-write");}
    OverrideOK=1;return 1;
}
static int EmptyKey(HKEY key)
{
    DWORD children=0,values=0;LONG result;
    result=RegQueryInfoKeyA(key,0,0,0,&children,0,0,&values,0,0,0,0);
    return result==ERROR_SUCCESS&&!children&&!values;
}
static void RestoreRegistry(void)
{
    LONG result;DWORD size,type;int removeGame=0,removeParent=0;
    /* No registry mutation may precede positive owned-tree stop evidence.
       A failed job query is not evidence that no descendants remain. */
    int safe=!CreatedProcess||ProcessTreeStopped;
    if(!safe){RestoreBlockedByProcessTree=1;Fail("registry-rollback-blocked-process-tree");}
    if(GameKey)
    {
        if(safe&&SnapshotOK&&OverrideAttempted)
        {
            RestoreAttempted=1;
            result=PreviousExists?RegSetValueExA(GameKey,"MediaPath",0,PreviousType,Previous,PreviousSize):RegDeleteValueA(GameKey,"MediaPath");
            if(result==ERROR_FILE_NOT_FOUND&&!PreviousExists)result=ERROR_SUCCESS;
            RestoreError=(DWORD)result;
            if(result==ERROR_SUCCESS)
            {
                size=sizeof(Verified);type=0;result=RegQueryValueExA(GameKey,"MediaPath",0,&type,Verified,&size);
                if(!PreviousExists&&result==ERROR_FILE_NOT_FOUND)RestoreVerified=1;
                else if(PreviousExists&&result==ERROR_SUCCESS&&type==PreviousType&&size==PreviousSize&&EqualBytes(Previous,Verified,size))RestoreVerified=1;
                else VerifyError=result==ERROR_SUCCESS?ERROR_INVALID_DATA:(DWORD)result;
            }
        }
        if(safe&&GameDisposition==REG_CREATED_NEW_KEY)
        { removeGame=EmptyKey(GameKey);if(!removeGame)GameDeleteError=ERROR_DIR_NOT_EMPTY; }
        result=RegCloseKey(GameKey);GameKey=0;if(result!=ERROR_SUCCESS)RegistryCloseError=(DWORD)result;
        if(removeGame){result=RegDeleteKeyA(ParentKey,"Winx Club");if(result!=ERROR_SUCCESS)GameDeleteError=(DWORD)result;}
    }
    if(ParentKey)
    {
        if(safe&&ParentDisposition==REG_CREATED_NEW_KEY)
        { removeParent=EmptyKey(ParentKey);if(!removeParent)ParentDeleteError=ERROR_DIR_NOT_EMPTY; }
        result=RegCloseKey(ParentKey);ParentKey=0;if(result!=ERROR_SUCCESS&&!RegistryCloseError)RegistryCloseError=(DWORD)result;
        if(removeParent){result=RegDeleteKeyA(HKEY_LOCAL_MACHINE,"Software\\Konami");if(result!=ERROR_SUCCESS)ParentDeleteError=(DWORD)result;}
    }
    if(RestoreError||VerifyError||(RestoreAttempted&&!RestoreVerified)||GameDeleteError||ParentDeleteError||RegistryCloseError)Fail("registry-rollback");
}
static int AcquireMediaLease(void)
{
    DWORD error;
    SetLastError(ERROR_SUCCESS);
    MediaLease=CreateEventA(0,TRUE,FALSE,"Local\\WinxCoop.MediaPathLease.v1");
    error=GetLastError();
    if(!MediaLease){MediaLeaseCreateError=error;return Fail("media-lease-create");}
    if(error==ERROR_ALREADY_EXISTS)
    {MediaLeaseCreateError=error;CloseHandle(MediaLease);MediaLease=0;return Fail("media-lease-already-owned");}
    MediaLeaseOwned=1;return 1;
}
static void ReleaseMediaLease(void)
{
    if(MediaLease)
    {
        if(CloseHandle(MediaLease))MediaLeaseClosed=1;
        else{MediaLeaseCloseError=GetLastError();Fail("media-lease-close");}
        MediaLease=0;
    }
}
static int PrepareChildJob(void)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    ChildJob=CreateJobObjectA(0,0);
    if(!ChildJob){JobCreateError=GetLastError();return Fail("child-job-create");}
    JobCreated=1;memset(&limits,0,sizeof(limits));
    /* Unnamed, non-inheritable handle; neither breakaway flag is enabled.
       All descendants remain in this job, even if the direct child exits. */
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(ChildJob,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
    {JobConfigureError=GetLastError();return Fail("child-job-configure");}
    JobConfigured=1;return 1;
}
static int ObserveChildJob(void)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
    memset(&accounting,0,sizeof(accounting));++JobQueryCalls;JobLastQuerySucceeded=0;JobActiveZero=0;
    if(!QueryInformationJobObject(ChildJob,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),0))
    {if(!JobQueryError)JobQueryError=GetLastError();Fail("child-job-query");return 0;}
    JobLastQuerySucceeded=1;
    if(!JobInitialObserved){JobInitialActive=accounting.ActiveProcesses;JobInitialObserved=1;}
    JobLastActive=accounting.ActiveProcesses;JobTotalAssigned=accounting.TotalProcesses;
    JobActiveZero=accounting.ActiveProcesses==0;return 1;
}
static void StopOwnedProcessTree(void)
{
    DWORD began,wait,elapsed;int observed;
    if(ProcessCleanupAttempted)return;
    ProcessCleanupAttempted=1;began=GetTickCount();
    if(ChildJob)
    {
        ObserveChildJob();JobTerminateAttempted=1;
        if(TerminateJobObject(ChildJob,124))
        {JobTerminated=1;if(CreatedProcess&&!ChildStopped&&JobAssigned)Terminated=1;}
        else{JobTerminateError=GetLastError();Fail("child-job-terminate");}
    }
    /* Assign failure cannot have descendants: the primary thread has NEVER
       been resumed. Kill and positively wait for this unassigned process. */
    if(CreatedProcess&&!JobAssigned&&!ChildStopped)
    {
        if(TerminateProcess(Child.hProcess,124))Terminated=1;
        else{TerminateError=GetLastError();Fail("own-suspended-child-terminate");}
    }
    for(;;)
    {
        if(CreatedProcess&&!ChildStopped)
        {
            wait=WaitForSingleObject(Child.hProcess,0);TerminationWait=wait;
            if(wait==WAIT_OBJECT_0)ChildStopped=1;
            else if(wait==WAIT_FAILED){if(!WaitError)WaitError=GetLastError();Fail("own-child-stop-wait");}
        }
        observed=ChildJob?ObserveChildJob():1;
        ProcessTreeStopped=(!CreatedProcess||ChildStopped)&&
            (!ChildJob||(observed&&JobActiveZero))&&(!CreatedProcess||JobAssigned||!ResumeAttempted);
        elapsed=(DWORD)(GetTickCount()-began);
        if(ProcessTreeStopped||elapsed>=5000)break;
        Sleep(10);
    }
    JobCleanupMs=(DWORD)(GetTickCount()-began);
    if(!ProcessTreeStopped)Fail("owned-process-tree-not-stopped");
    if(CreatedProcess)
    {
        if(!GetExitCodeProcess(Child.hProcess,&ExitCode)){ExitQueryError=GetLastError();Fail("game-final-exit-query");}
        if(!CloseHandle(Child.hThread))CloseThreadError=GetLastError();Child.hThread=0;
        if(!CloseHandle(Child.hProcess))CloseProcessError=GetLastError();Child.hProcess=0;
        if(CloseThreadError||CloseProcessError)Fail("child-handle-close");
    }
    if(ChildJob)
    {
        /* Last-handle close is a second kill safeguard, not a substitute for
           ActiveProcesses==0. Unconfirmed cleanup still blocks rollback. */
        if(CloseHandle(ChildJob))JobClosed=1;
        else{JobCloseError=GetLastError();Fail("child-job-close");}
        ChildJob=0;
    }
}
static void BrokerDecimal(char* output,DWORD value)
{
    char digits[10];DWORD count=0,i=0;
    do{digits[count++]=(char)('0'+value%10);value/=10;}while(value);
    while(count)output[i++]=digits[--count];output[i]=0;
}
static void BrokerHexTime(char* output,const FILETIME* value)
{
    static const char hex[]="0123456789ABCDEF";DWORD i;
    for(i=0;i<8;++i){output[i]=hex[(value->dwHighDateTime>>((7-i)*4))&15];output[i+8]=hex[(value->dwLowDateTime>>((7-i)*4))&15];}output[16]=0;
}
static int BrokerHexEqual(const char* value,const char* expected)
{
    DWORD i;if(Length(value)!=16)return 0;
    for(i=0;i<16;++i)if(UpperAscii(value[i])!=expected[i])return 0;
    return 1;
}
static int BrokerLine(const char* text)
{
    DWORD size=Length(text);
    if(BrokerFrameBytes+size+2>=sizeof(BrokerFrame))return BrokerFail("media-broker-frame-bound",ERROR_INSUFFICIENT_BUFFER);
    memcpy(BrokerFrame+BrokerFrameBytes,text,size);BrokerFrameBytes+=size;
    BrokerFrame[BrokerFrameBytes++]='\r';BrokerFrame[BrokerFrameBytes++]='\n';return 1;
}
static int BrokerIdentityFrame(const char* marker)
{
    BrokerFrameBytes=0;
    return BrokerLine(marker)&&BrokerLine(BrokerParentPid)&&BrokerLine(BrokerParentTime)&&
        BrokerLine(BrokerChildPid)&&BrokerLine(BrokerChildTime);
}
static DWORD BrokerWriteFresh(const char* path)
{
    HANDLE file;DWORD error=0,written=0;BOOL ok;
    file=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE)return GetLastError();
    ok=WriteFile(file,BrokerFrame,BrokerFrameBytes,&written,0);
    if(!ok)error=GetLastError();else if(written!=BrokerFrameBytes)error=ERROR_WRITE_FAULT;
    if(!CloseHandle(file)){BrokerCloseError=GetLastError();if(!error)error=BrokerCloseError;}
    return error;
}
static DWORD BrokerReadEffective(void)
{
    LONG result;DWORD i;
    BrokerActual[0]=0;BrokerActualType=0;BrokerActualBytes=sizeof(BrokerActual);
    result=RegQueryValueExA(GameKey,"MediaPath",0,&BrokerActualType,(BYTE*)BrokerActual,&BrokerActualBytes);
    if(result!=ERROR_SUCCESS){BrokerActual[0]=0;return (DWORD)result;}
    if(BrokerActualType!=REG_SZ||!BrokerActualBytes||BrokerActualBytes>sizeof(BrokerActual)||
        BrokerActual[BrokerActualBytes-1]!=0){BrokerActual[0]=0;return ERROR_INVALID_DATA;}
    for(i=0;i+1<BrokerActualBytes;++i)
        if((unsigned char)BrokerActual[i]<32||(unsigned char)BrokerActual[i]>126){BrokerActual[0]=0;return ERROR_INVALID_DATA;}
    if(!Absolute(BrokerActual)||Length(BrokerActual)+1!=BrokerActualBytes){BrokerActual[0]=0;return ERROR_INVALID_DATA;}
    return ERROR_SUCCESS;
}
static int BrokerWriteInitialState(void)
{
    FILETIME parentCreated,childCreated,exited,kernel,user;DWORD error;
    if(!HaveNextMedia)return 1;
    if(!BrokerPathsReady||!GameKey||!SnapshotOK||!OverrideOK||!MediaLeaseOwned||!CreatedProcess||!JobAssigned||ResumeAttempted)
        return BrokerFail("media-broker-state-owner",ERROR_INVALID_DATA);
    if(!GetProcessTimes(GetCurrentProcess(),&parentCreated,&exited,&kernel,&user)||
        !GetProcessTimes(Child.hProcess,&childCreated,&exited,&kernel,&user))return BrokerFail("media-broker-process-times",GetLastError());
    BrokerDecimal(BrokerParentPid,GetCurrentProcessId());BrokerDecimal(BrokerChildPid,ProcessId);
    BrokerHexTime(BrokerParentTime,&parentCreated);BrokerHexTime(BrokerChildTime,&childCreated);
    error=BrokerReadEffective();BrokerReadError=error;
    if(error||!Equal(BrokerActual,Media))return BrokerFail("media-broker-host-readback",error?error:ERROR_INVALID_DATA);
    BrokerHostReadback=1;
    if(!BrokerIdentityFrame("WCMEDIA1")||!BrokerLine(Media)||!BrokerLine(NextMedia))return 0;
    error=BrokerWriteFresh(BrokerState);if(error)return BrokerFail("media-broker-state-write",error);
    BrokerStateWritten=1;return 1;
}
static DWORD BrokerReadRequest(char** lines)
{
    HANDLE file;DWORD size,high=0,read=0,error=0,i,start=0,count=0;BOOL ok;
    file=CreateFileA(BrokerRequest,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE)return GetLastError();
    SetLastError(ERROR_SUCCESS);size=GetFileSize(file,&high);
    if(high||!size||size>=sizeof(BrokerInput)||(size==INVALID_FILE_SIZE&&GetLastError()!=ERROR_SUCCESS))error=ERROR_INVALID_DATA;
    if(!error)
    {ok=ReadFile(file,BrokerInput,size,&read,0);if(!ok)error=GetLastError();else if(read!=size)error=ERROR_READ_FAULT;}
    if(!CloseHandle(file)){BrokerCloseError=GetLastError();if(!error)error=BrokerCloseError;}
    if(error)return error;
    BrokerRequestBytes=size;BrokerInput[size]=0;
    for(i=0;i<size;++i)
    {
        unsigned char c=(unsigned char)BrokerInput[i];
        if(c=='\r'||c=='\n')
        {
            if(i==start||count>=5)return ERROR_INVALID_DATA;
            lines[count++]=BrokerInput+start;BrokerInput[i]=0;
            if(c=='\r'){if(i+1>=size||BrokerInput[i+1]!='\n')return ERROR_INVALID_DATA;BrokerInput[++i]=0;}
            start=i+1;
        }
        else if(c<32||c>126)return ERROR_INVALID_DATA;
    }
    if(start<size){if(count>=5)return ERROR_INVALID_DATA;lines[count++]=BrokerInput+start;}
    if(count!=5||!Equal(lines[0],"WCSWITCH1")||!Equal(lines[1],BrokerParentPid)||
        !BrokerHexEqual(lines[2],BrokerParentTime)||!Equal(lines[3],BrokerChildPid)||
        !BrokerHexEqual(lines[4],BrokerChildTime))return ERROR_INVALID_DATA;
    return ERROR_SUCCESS;
}
static int BrokerWriteAcknowledgement(int succeeded,DWORD error)
{
    DWORD result;char number[11];
    if(!BrokerIdentityFrame("WCACK1")||!BrokerLine(succeeded?"1":"0"))return 0;
    BrokerDecimal(number,error);
    if(!BrokerLine(number)||!BrokerLine(BrokerActual))return 0;
    result=BrokerWriteFresh(BrokerAckPending);
    if(!result&&!MoveFileA(BrokerAckPending,BrokerAck))result=GetLastError();
    if(result){BrokerAckError=result;return BrokerFail("media-broker-ack-publish",result);}
    BrokerAckWritten=1;return 1;
}
static int BrokerPollRequest(void)
{
    DWORD attributes,error,readError;char* lines[5];LONG result;
    ++BrokerPolls;
    if(BrokerRequestSeen)return BrokerError==0;
    attributes=GetFileAttributesA(BrokerRequest);
    if(attributes==INVALID_FILE_ATTRIBUTES)
    {
        error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return 1;
        return BrokerFail("media-broker-request-inspect",error);
    }
    BrokerRequestSeen=1;
    error=(attributes&FILE_ATTRIBUTE_DIRECTORY)?ERROR_INVALID_DATA:BrokerReadRequest(lines);
    if(!error&&(!BrokerFileAbsent(BrokerAck)||!BrokerFileAbsent(BrokerAckPending)))error=BrokerError;
    if(!error)
    {
        BrokerRequestValid=1;readError=BrokerReadEffective();BrokerReadError=readError;
        if(readError||!Equal(BrokerActual,Media))error=readError?readError:ERROR_INVALID_DATA;
        else BrokerHostBeforeSwitchVerified=1;
    }
    if(!error)
    {
        BrokerSwitchAttempted=1;
        result=RegSetValueExA(GameKey,"MediaPath",0,REG_SZ,(const BYTE*)NextMedia,Length(NextMedia)+1);
        BrokerSetError=(DWORD)result;if(result!=ERROR_SUCCESS)error=(DWORD)result;
        readError=BrokerReadEffective();BrokerReadError=readError;
        if(!error&&(readError||!Equal(BrokerActual,NextMedia)))error=readError?readError:ERROR_INVALID_DATA;
        if(!error){BrokerGuestReadback=1;BrokerSwitchSucceeded=1;}
    }
    else
    {
        /* An error ACK reports a FRESH same-handle read when possible; an
           unreadable value is empty, never stale state advertised as current. */
        readError=BrokerReadEffective();BrokerReadError=readError;
    }
    if(error)BrokerFail("media-broker-switch",error);
    if(!BrokerWriteAcknowledgement(!error,error))return 0;
    return !error;
}
static void BrokerWaitChild(DWORD began)
{
    DWORD elapsed,remaining,slice;
    for(;;)
    {
        WaitResult=WaitForSingleObject(Child.hProcess,0);
        if(WaitResult==WAIT_OBJECT_0||WaitResult==WAIT_FAILED)return;
        elapsed=(DWORD)(GetTickCount()-began);
        if(elapsed>=ObservationLimitMs){WaitResult=WAIT_TIMEOUT;return;}
        if(!BrokerPollRequest()){WaitError=BrokerError;WaitResult=WAIT_FAILED;return;}
        remaining=ObservationLimitMs-elapsed;slice=remaining<20?remaining:20;
        WaitResult=WaitForSingleObject(Child.hProcess,slice);
        if(WaitResult==WAIT_OBJECT_0||WaitResult==WAIT_FAILED)return;
    }
}
static void LaunchGame(void)
{
    DWORD began;BOOL ok;
    if(!PrepareChildJob()){StopOwnedProcessTree();return;}
    Startup.cb=sizeof(Startup);
    ok=CreateProcessA(Game,Command,0,0,FALSE,CREATE_SUSPENDED,0,Directory,&Startup,&Child);
    if(!ok){LaunchError=GetLastError();Fail("game-create-process");StopOwnedProcessTree();return;}
    CreatedProcess=1;ProcessId=Child.dwProcessId;
    if(!AssignProcessToJobObject(ChildJob,Child.hProcess))
    {JobAssignError=GetLastError();Fail("child-job-assign");StopOwnedProcessTree();return;}
    JobAssigned=1;
    if(!BrokerWriteInitialState()){StopOwnedProcessTree();return;}
    ResumeAttempted=1;ResumeResult=ResumeThread(Child.hThread);
    if(ResumeResult!=1)
    {if(ResumeResult==0xffffffffUL)ResumeError=GetLastError();Fail("child-primary-thread-resume");StopOwnedProcessTree();return;}
    ChildResumed=1;began=GetTickCount();
    /* Bounded observation, then at most five seconds of entire-tree cleanup. */
    if(HaveNextMedia)BrokerWaitChild(began);
    else WaitResult=WaitForSingleObject(Child.hProcess,ObservationLimitMs);
    ObserveMs=(DWORD)(GetTickCount()-began);
    if(WaitResult==WAIT_FAILED){if(!WaitError)WaitError=GetLastError();Fail("game-wait");}
    if(!GetExitCodeProcess(Child.hProcess,&ExitCode)){ExitQueryError=GetLastError();Fail("game-exit-query");}
    if(WaitResult==WAIT_OBJECT_0)ChildStopped=1;
    else TimedOut=WaitResult==WAIT_TIMEOUT;
    StopOwnedProcessTree();
}
static void Append(const char* text)
{ while(*text&&JsonSize+1<sizeof(Json))Json[JsonSize++]=*text++;if(*text)FileError=ERROR_INSUFFICIENT_BUFFER; }
static void Number(DWORD value)
{
    char digits[10],one[2];unsigned n=0;
    do{digits[n++]=(char)('0'+value%10);value/=10;}while(value&&n<sizeof(digits));
    one[1]=0;while(n){one[0]=digits[--n];Append(one);}
}
static void String(const char* text)
{
    static const char hex[]="0123456789abcdef";char escaped[7];unsigned char c;
    Append("\"");while(*text)
    {
        c=(unsigned char)*text++;
        if(c=='"')Append("\\\"");else if(c=='\\')Append("\\\\");
        else if(c<32||c>=127){memcpy(escaped,"\\u00",4);escaped[4]=hex[c>>4];escaped[5]=hex[c&15];escaped[6]=0;Append(escaped);}
        else{escaped[0]=(char)c;escaped[1]=0;Append(escaped);}
    }Append("\"");
}
static void N(const char* name,DWORD value)
{Append(",");String(name);Append(":");Number(value);}
static void B(const char* name,int value)
{Append(",");String(name);Append(value?":true":":false");}
static void S(const char* name,const char* value)
{Append(",");String(name);Append(":");String(value);}
static void Caps(const char* name,const D3DCAPS9* caps,int called,HRESULT hr)
{
    Append(",");String(name);Append(":{\"called\":");Append(called?"true":"false");N("hr",(DWORD)hr);
    if(called&&SUCCEEDED(hr))
    {N("deviceType",caps->DeviceType);N("devCaps",caps->DevCaps);N("vertexShaderVersion",caps->VertexShaderVersion);N("pixelShaderVersion",caps->PixelShaderVersion);N("maxTextureWidth",caps->MaxTextureWidth);N("maxTextureHeight",caps->MaxTextureHeight);N("maxSimultaneousTextures",caps->MaxSimultaneousTextures);}
    Append("}");
}
static int WriteReport(void)
{
    DWORD written=0;BOOL ok;int completed=CreatedProcess&&ChildStopped&&ProcessTreeStopped&&RestoreVerified&&Equal(Failure,"none");
    Append("{\"kind\":\"XP32-original-game-graphics-probe\",\"testOnly\":true,\"probeCompleted\":");Append(completed?"true":"false");
    S("failure",Failure);N("pid",GetCurrentProcessId());N("elapsedMs",(DWORD)(GetTickCount()-Began));B("sidecarUsed",SidecarUsed);N("argumentError",ArgumentError);S("game",Game);S("media",Media);
    Append(",\"os\":{\"querySucceeded\":");Append(VersionOK?"true":"false");N("major",Version.dwMajorVersion);N("minor",Version.dwMinorVersion);N("build",Version.dwBuildNumber);N("servicePackMajor",Version.wServicePackMajor);Append("}");
    Append(",\"graphics\":{\"create9ReturnedInterface\":");Append(D3D?"true":"false");S("requestedModule",D3DPath);S("loadedModule",LoadedD3DPath);N("loadError",D3DError);N("modulePathError",ModulePathError);N("sdkVersion",D3D_SDK_VERSION);
    B("identifierCalled",IdentifierCalled);N("identifierHR",(DWORD)IdentifierHR);
    if(IdentifierCalled&&SUCCEEDED(IdentifierHR)){S("adapterDriver",Identifier.Driver);S("adapterDescription",Identifier.Description);N("vendorId",Identifier.VendorId);N("deviceId",Identifier.DeviceId);N("driverVersionHigh",Identifier.DriverVersion.HighPart);N("driverVersionLow",Identifier.DriverVersion.LowPart);}
    B("displayCalled",DisplayCalled);N("displayHR",(DWORD)DisplayHR);if(DisplayCalled&&SUCCEEDED(DisplayHR)){N("width",Mode.Width);N("height",Mode.Height);N("format",Mode.Format);}
    Caps("HAL",&HalCaps,HalCapsCalled,HalCapsHR);Caps("REF",&RefCaps,RefCapsCalled,RefCapsHR);
    B("halWindowedCheckCalled",HalCheckCalled);N("halWindowedCheckHR",(DWORD)HalCheckHR);B("refWindowedCheckCalled",RefCheckCalled);N("refWindowedCheckHR",(DWORD)RefCheckHR);N("releasedReferenceCount",ReleasedRefs);N("freeLibraryError",FreeLibraryError);Append("}");
    Append(",\"registry\":{\"onlyValue\":");String("HKLM\\Software\\Konami\\Winx Club\\MediaPath");
    N("parentDisposition",ParentDisposition);N("gameDisposition",GameDisposition);N("openError",RegistryOpenError);B("snapshotOK",SnapshotOK);B("previousValueExisted",PreviousExists);N("previousType",PreviousType);N("previousBytes",PreviousSize);N("snapshotError",SnapshotError);B("overrideAttempted",OverrideAttempted);B("overrideOK",OverrideOK);N("overrideError",OverrideError);B("restoreAttempted",RestoreAttempted);B("restoreVerified",RestoreVerified);B("restoreBlockedByProcessTree",RestoreBlockedByProcessTree);N("restoreError",RestoreError);N("verifyError",VerifyError);N("gameKeyDeleteError",GameDeleteError);N("parentKeyDeleteError",ParentDeleteError);N("closeError",RegistryCloseError);B("languageQueried",LanguageQueried);N("languageQueryStatus",LanguageStatus);N("languageType",LanguageType);N("languageBytes",LanguageBytes);N("languageValue",LanguageValue);Append("}");
    Append(",\"job\":{\"created\":");Append(JobCreated?"true":"false");B("configuredKillOnClose",JobConfigured);B("assignedBeforeResume",JobAssigned);B("resumeAttempted",ResumeAttempted);B("childResumed",ChildResumed);N("resumeResult",ResumeResult);N("resumeError",ResumeError);B("cleanupAttempted",ProcessCleanupAttempted);B("terminateAttempted",JobTerminateAttempted);B("terminateSucceeded",JobTerminated);N("queryCalls",JobQueryCalls);B("initialQuerySucceeded",JobInitialObserved);B("lastQuerySucceeded",JobLastQuerySucceeded);N("initialActiveProcesses",JobInitialActive);N("lastActiveProcesses",JobLastActive);N("totalAssignedProcesses",JobTotalAssigned);B("activeProcessesZeroVerified",JobActiveZero);B("ownedTreeStopped",ProcessTreeStopped);N("cleanupMs",JobCleanupMs);B("handleClosed",JobClosed);N("createError",JobCreateError);N("configureError",JobConfigureError);N("assignError",JobAssignError);N("terminateError",JobTerminateError);N("queryError",JobQueryError);N("closeError",JobCloseError);Append("}");
    Append(",\"mediaLease\":{\"owned\":");Append(MediaLeaseOwned?"true":"false");B("requested",OwnMediaLeaseRequested);B("closed",MediaLeaseClosed);N("createError",MediaLeaseCreateError);N("closeError",MediaLeaseCloseError);Append("}");
    Append(",\"mediaBroker\":{\"enabled\":");Append(HaveNextMedia?"true":"false");
    B("pathsPreflighted",BrokerPathsReady);B("stateWrittenBeforeResume",BrokerStateWritten);B("initialHostReadback",BrokerHostReadback);
    B("requestSeen",BrokerRequestSeen);B("requestValid",BrokerRequestValid);B("hostBeforeSwitchVerified",BrokerHostBeforeSwitchVerified);
    B("guestSetAttempted",BrokerSwitchAttempted);B("guestSetAndReadbackSucceeded",BrokerSwitchSucceeded);B("guestReadback",BrokerGuestReadback);B("ackPublished",BrokerAckWritten);
    N("error",BrokerError);N("lastReadError",BrokerReadError);N("setError",BrokerSetError);N("ackError",BrokerAckError);N("fileCloseError",BrokerCloseError);
    N("polls",BrokerPolls);N("requestBytes",BrokerRequestBytes);N("lastEffectiveType",BrokerActualType);N("lastEffectiveBytes",BrokerActualBytes);
    S("parentPidDecimal",BrokerParentPid);S("parentCreationHex16",BrokerParentTime);S("childPidDecimal",BrokerChildPid);S("childCreationHex16",BrokerChildTime);
    S("stateFile",BrokerState);S("requestFile",BrokerRequest);S("ackFile",BrokerAck);S("hostMedia",Media);S("guestMedia",NextMedia);S("lastEffectiveMediaPathAtBrokerRead",BrokerActual);Append("}");
    Append(",\"child\":{\"created\":");Append(CreatedProcess?"true":"false");N("pid",ProcessId);N("createError",LaunchError);N("observationLimitMs",ObservationLimitMs);N("observationMs",ObserveMs);N("waitResult",WaitResult);N("waitError",WaitError);N("exitCode",ExitCode);N("exitQueryError",ExitQueryError);B("timedOut",TimedOut);B("terminateSucceeded",Terminated);N("terminateError",TerminateError);N("terminationWaitResult",TerminationWait);B("stopped",ChildStopped);N("closeThreadError",CloseThreadError);N("closeProcessError",CloseProcessError);Append("},\"gameplayAccepted\":false}\r\n");
    if(ReportFile==INVALID_HANDLE_VALUE)return 0;
    if(FileError){CloseHandle(ReportFile);ReportFile=INVALID_HANDLE_VALUE;return 0;}
    ok=WriteFile(ReportFile,Json,JsonSize,&written,0);if(!ok)FileError=GetLastError();
    if(!CloseHandle(ReportFile)&&!FileError)FileError=GetLastError();ReportFile=INVALID_HANDLE_VALUE;
    return ok&&written==JsonSize&&!FileError;
}
__declspec(noreturn) void __cdecl ProbeEntry(void)
{
    int completed;
    Began=GetTickCount();Version.dwOSVersionInfoSize=sizeof(Version);VersionOK=GetVersionExA((OSVERSIONINFOA*)&Version)!=0;
    /* The P launcher already owns/inherits this event. Only a standalone
       supervisor explicitly asks to become the new lease owner. */
    if(Arguments()&&(!OwnMediaLeaseRequested||AcquireMediaLease())){Graphics();if(PrepareRegistry())LaunchGame();}
    RestoreRegistry();
    if(D3D)ReleasedRefs=IDirect3D9_Release(D3D);
    if(D3DModule&&!FreeLibrary(D3DModule)){FreeLibraryError=GetLastError();Fail("d3d-library-close");}
    ReleaseMediaLease();
    completed=CreatedProcess&&ChildStopped&&ProcessTreeStopped&&RestoreVerified&&Equal(Failure,"none");
    if(!WriteReport())ExitProcess(3);
    ExitProcess(completed?0:4);
}
