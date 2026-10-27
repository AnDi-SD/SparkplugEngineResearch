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
static DWORD ObservationLimitMs=40000;
static int HaveGame,HaveMedia,HaveReport,SidecarUsed,VersionOK,SnapshotOK,PreviousExists;
static int HaveObserve;
static int OverrideAttempted,OverrideOK,RestoreAttempted,RestoreVerified,CreatedProcess,TimedOut,Terminated,ChildStopped;
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
    if(value<1000||value>900000)return Fail("observe-ms-range");
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
        if(++count>4)return Fail("argument-count");
        if(Equal(Token,"--game")){if(HaveGame||Next(&p,Game,sizeof(Game))!=1)return Fail("game-argument");HaveGame=1;}
        else if(Equal(Token,"--media")){if(HaveMedia||Next(&p,Media,sizeof(Media))!=1)return Fail("media-argument");HaveMedia=1;}
        else if(Equal(Token,"--report")){if(HaveReport||Next(&p,Report,sizeof(Report))!=1)return Fail("report-argument");HaveReport=1;}
        else if(Equal(Token,"--observe-ms")){if(HaveObserve||Next(&p,Token,sizeof(Token))!=1||!ParseObservationLimit(Token))return Fail("observe-ms-argument");HaveObserve=1;}
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
    return 1;
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
    if(GameKey)
    {
        if(SnapshotOK&&OverrideAttempted)
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
        if(GameDisposition==REG_CREATED_NEW_KEY)
        { removeGame=EmptyKey(GameKey);if(!removeGame)GameDeleteError=ERROR_DIR_NOT_EMPTY; }
        result=RegCloseKey(GameKey);GameKey=0;if(result!=ERROR_SUCCESS)RegistryCloseError=(DWORD)result;
        if(removeGame){result=RegDeleteKeyA(ParentKey,"Winx Club");if(result!=ERROR_SUCCESS)GameDeleteError=(DWORD)result;}
    }
    if(ParentKey)
    {
        if(ParentDisposition==REG_CREATED_NEW_KEY)
        { removeParent=EmptyKey(ParentKey);if(!removeParent)ParentDeleteError=ERROR_DIR_NOT_EMPTY; }
        result=RegCloseKey(ParentKey);ParentKey=0;if(result!=ERROR_SUCCESS&&!RegistryCloseError)RegistryCloseError=(DWORD)result;
        if(removeParent){result=RegDeleteKeyA(HKEY_LOCAL_MACHINE,"Software\\Konami");if(result!=ERROR_SUCCESS)ParentDeleteError=(DWORD)result;}
    }
    if(RestoreError||VerifyError||(RestoreAttempted&&!RestoreVerified)||GameDeleteError||ParentDeleteError||RegistryCloseError)Fail("registry-rollback");
}
static void LaunchGame(void)
{
    DWORD began;BOOL ok;
    Startup.cb=sizeof(Startup);
    ok=CreateProcessA(Game,Command,0,0,FALSE,0,0,Directory,&Startup,&Child);
    if(!ok){LaunchError=GetLastError();Fail("game-create-process");return;}
    CreatedProcess=1;ProcessId=Child.dwProcessId;began=GetTickCount();
    /* Bounded configured observation, then at most five seconds of own-child cleanup. */
    WaitResult=WaitForSingleObject(Child.hProcess,ObservationLimitMs);ObserveMs=(DWORD)(GetTickCount()-began);
    if(WaitResult==WAIT_FAILED){WaitError=GetLastError();Fail("game-wait");}
    if(!GetExitCodeProcess(Child.hProcess,&ExitCode)){ExitQueryError=GetLastError();Fail("game-exit-query");}
    if(WaitResult==WAIT_OBJECT_0)ChildStopped=1;
    else
    {
        TimedOut=WaitResult==WAIT_TIMEOUT;
        if(TerminateProcess(Child.hProcess,124))Terminated=1;
        else{TerminateError=GetLastError();Fail("own-child-terminate");}
        TerminationWait=WaitForSingleObject(Child.hProcess,5000);
        if(TerminationWait==WAIT_OBJECT_0)ChildStopped=1;
        else Fail("own-child-not-stopped");
        if(!GetExitCodeProcess(Child.hProcess,&ExitCode)){ExitQueryError=GetLastError();Fail("game-final-exit-query");}
    }
    if(!CloseHandle(Child.hThread))CloseThreadError=GetLastError();Child.hThread=0;
    if(!CloseHandle(Child.hProcess))CloseProcessError=GetLastError();Child.hProcess=0;
    if(CloseThreadError||CloseProcessError)Fail("child-handle-close");
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
    DWORD written=0;BOOL ok;int completed=CreatedProcess&&ChildStopped&&RestoreVerified&&Equal(Failure,"none");
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
    N("parentDisposition",ParentDisposition);N("gameDisposition",GameDisposition);N("openError",RegistryOpenError);B("snapshotOK",SnapshotOK);B("previousValueExisted",PreviousExists);N("previousType",PreviousType);N("previousBytes",PreviousSize);N("snapshotError",SnapshotError);B("overrideAttempted",OverrideAttempted);B("overrideOK",OverrideOK);N("overrideError",OverrideError);B("restoreAttempted",RestoreAttempted);B("restoreVerified",RestoreVerified);N("restoreError",RestoreError);N("verifyError",VerifyError);N("gameKeyDeleteError",GameDeleteError);N("parentKeyDeleteError",ParentDeleteError);N("closeError",RegistryCloseError);B("languageQueried",LanguageQueried);N("languageQueryStatus",LanguageStatus);N("languageType",LanguageType);N("languageBytes",LanguageBytes);N("languageValue",LanguageValue);Append("}");
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
    if(Arguments()){Graphics();if(PrepareRegistry())LaunchGame();}
    RestoreRegistry();
    if(D3D)ReleasedRefs=IDirect3D9_Release(D3D);
    if(D3DModule&&!FreeLibrary(D3DModule)){FreeLibraryError=GetLastError();Fail("d3d-library-close");}
    completed=CreatedProcess&&ChildStopped&&RestoreVerified&&Equal(Failure,"none");
    if(!WriteReport())ExitProcess(3);
    ExitProcess(completed?0:4);
}
