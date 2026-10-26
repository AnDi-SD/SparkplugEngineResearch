/* Independent, test-only XP/x86 socket oracle. This is not a production
   WinxCoop port or a second production serializer. No CRT/heap is used. */
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <stddef.h>
#include <string.h> /* Declarations only; the two helpers below are ours. */
#if !defined(_M_IX86)
#error XpSocketPeer must be compiled with the x86 compiler.
#endif

/* Volatile loops prevent an optimizing compiler from recursively replacing
   these freestanding helpers with CRT calls. */
#pragma function(memcpy, memset)
void* __cdecl memcpy(void* output, const void* input, size_t size)
{
    volatile unsigned char* to = (volatile unsigned char*)output;
    const volatile unsigned char* from = (const volatile unsigned char*)input;
    size_t i; for(i = 0; i < size; ++i) to[i] = from[i]; return output;
}
void* __cdecl memset(void* output, int value, size_t size)
{
    volatile unsigned char* to = (volatile unsigned char*)output;
    size_t i; for(i = 0; i < size; ++i) to[i] = (unsigned char)value; return output;
}

static const unsigned char Probe[18] = {0xd0,0xba,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0x2a};
static const unsigned char Info[19] = {0,0,0xff,0xff,1,0,1,0,0,0,0,0,0,0,0,2,0,1,0};
static const unsigned char Ready[18] = {1,0,0,0,3,0,1,0,0,0,0,0,0,0,0,1,0,1};
static const unsigned char Start[21] = {0,0,1,0,4,0,1,0,0,0,0,0,0,0,0,4,0,1,0,0x34,0x12};
static const unsigned short Sizes[3] = {0,1,256};
static SOCKET SocketValue = INVALID_SOCKET;
static WSADATA Wsa;
static OSVERSIONINFOEXA Version;
static struct sockaddr_in Server, Local, Remote;
static char Argument[512], ServerText[64], ReportPath[MAX_PATH];
static char SidecarPath[MAX_PATH], Sidecar[1025];
static char Json[4096];
static unsigned char ReceiveFrame[273], SendFrame[273], PatternBytes[256];
static DWORD JsonSize, Began, SendCalls, ReceiveCalls, BytesSent, BytesReceived;
static DWORD WsaStarts, WsaCleanups, SocketsOpened, SocketsClosed;
static DWORD RequestsSent, RequestsReceived, RepliesSent, RepliesReceived;
static DWORD HostHelloRequests, HostHelloReplies, ConnectPolls, FrameCount;
static DWORD OwnReplyMask, HostRequestMask, PongSecondWord;
static int FirstSocketError, LastSocketError, StartupError, CloseError, CleanupError;
static DWORD VersionError, FileError, ConfigurationError;
static int HaveReport, GotStart, GotPong, PingSent, SawEof, VersionOk, Passed, SidecarUsed;
static const char* Failure = "none";

static int EqualText(const char* left, const char* right)
{ while(*left && *left == *right) { ++left; ++right; } return *left == *right; }
static int EqualBytes(const unsigned char* left, const unsigned char* right, DWORD size)
{ DWORD i; for(i=0;i<size;++i) if(left[i]!=right[i]) return 0; return 1; }
static int Fail(const char* reason)
{ if(EqualText(Failure,"none")) Failure=reason; return 0; }
static int SocketFail(const char* reason)
{
    LastSocketError=WSAGetLastError();
    if(!FirstSocketError) FirstSocketError=LastSocketError;
    return Fail(reason);
}
static int Alive(void) { return (DWORD)(GetTickCount()-Began) < 20000; }
static unsigned short Read16(const unsigned char* bytes)
{ return (unsigned short)(bytes[0] | ((unsigned short)bytes[1]<<8)); }
static DWORD Read32(const unsigned char* bytes)
{ return (DWORD)bytes[0] | ((DWORD)bytes[1]<<8) | ((DWORD)bytes[2]<<16) | ((DWORD)bytes[3]<<24); }
static void Put16(unsigned char* bytes, unsigned short value)
{ bytes[0]=(unsigned char)value; bytes[1]=(unsigned char)(value>>8); }
static void Put32(unsigned char* bytes, DWORD value)
{ unsigned i; for(i=0;i<4;++i) bytes[i]=(unsigned char)(value>>(8*i)); }
static int IsSpace(char value)
{ return value==' ' || value=='\t' || value=='\r' || value=='\n'; }

static int NextToken(const char** command, char* output, DWORD capacity)
{
    const char* at=*command; DWORD used=0; int quoted;
    while(IsSpace(*at)) ++at;
    if(!*at) { *command=at; return 0; }
    quoted=*at=='"'; if(quoted) ++at;
    while(*at && (quoted ? *at!='"' : !IsSpace(*at)))
    {
        unsigned char value=(unsigned char)*at++;
        if(value<32 || value>126 || value=='"' || used+1>=capacity) return -1;
        output[used++]=(char)value;
    }
    if(quoted) { if(*at!='"') return -1; ++at; if(*at && !IsSpace(*at)) return -1; }
    output[used]=0; *command=at; return 1;
}
static int ParseServer(void)
{
    const char* at=ServerText; DWORD address=0,port=0,value; unsigned part,digits;
    for(part=0;part<4;++part)
    {
        value=0; digits=0;
        while(*at>='0' && *at<='9')
        { if(++digits>3) return 0; value=value*10+(DWORD)(*at++-'0'); if(value>255) return 0; }
        if(!digits || *at!=(part==3?':':'.')) return 0;
        ++at; address=(address<<8)|value;
    }
    digits=0;
    while(*at>='0' && *at<='9')
    { if(++digits>5) return 0; port=port*10+(DWORD)(*at++-'0'); if(port>65535) return 0; }
    if(*at || !digits || !port || !address || address==0xffffffffUL) return 0;
    Server.sin_family=AF_INET; Server.sin_addr.s_addr=htonl(address); Server.sin_port=htons((unsigned short)port);
    return 1;
}
static int LoadSidecar(void)
{
    HANDLE file; DWORD pathSize,size,high=0,read=0,i; BOOL success;
    pathSize=GetModuleFileNameA(0,SidecarPath,sizeof(SidecarPath));
    if(!pathSize) { ConfigurationError=GetLastError(); return Fail("sidecar-module-path"); }
    if(pathSize+5>=sizeof(SidecarPath)) return Fail("sidecar-path-bound");
    memcpy(SidecarPath+pathSize,".args",6);
    file=CreateFileA(SidecarPath,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE) { ConfigurationError=GetLastError(); return Fail("sidecar-open"); }
    SetLastError(NO_ERROR); size=GetFileSize(file,&high);
    if(size==INVALID_FILE_SIZE && GetLastError()!=NO_ERROR)
    { ConfigurationError=GetLastError(); CloseHandle(file); return Fail("sidecar-size"); }
    if(high || !size || size>=sizeof(Sidecar)) { CloseHandle(file); return Fail("sidecar-size-bound"); }
    success=ReadFile(file,Sidecar,size,&read,0);
    if(!success) ConfigurationError=GetLastError();
    if(!CloseHandle(file) && !ConfigurationError) ConfigurationError=GetLastError();
    if(!success || read!=size || ConfigurationError) return Fail("sidecar-read");
    for(i=0;i<read;++i)
        if(((unsigned char)Sidecar[i]<32 && !IsSpace(Sidecar[i])) || (unsigned char)Sidecar[i]>126)
            return Fail("sidecar-ascii");
    Sidecar[read]=0; SidecarUsed=1; return 1;
}
static int Arguments(void)
{
    const char* command=GetCommandLineA(); int token,seenServer=0; unsigned count=0;
    if(NextToken(&command,Argument,sizeof(Argument))!=1) return Fail("command-line");
    token=NextToken(&command,Argument,sizeof(Argument));
    if(!token)
    { if(!LoadSidecar()) return 0; command=Sidecar; token=NextToken(&command,Argument,sizeof(Argument)); }
    while(token==1)
    {
        if(++count>2) return Fail("argument-count");
        if(EqualText(Argument,"--server"))
        { if(seenServer || NextToken(&command,ServerText,sizeof(ServerText))!=1) return Fail("server-argument"); seenServer=1; }
        else if(EqualText(Argument,"--report"))
        { if(HaveReport || NextToken(&command,ReportPath,sizeof(ReportPath))!=1) return Fail("report-argument"); HaveReport=1; }
        else return Fail("unknown-argument");
        token=NextToken(&command,Argument,sizeof(Argument));
    }
    if(token<0 || !HaveReport || !seenServer || !ReportPath[0] || !ParseServer()) return Fail("arguments");
    return 1;
}
static int SendExact(const unsigned char* bytes,DWORD size)
{
    DWORD offset=0,attempts=0; int result;
    while(offset<size && ++attempts<=1024)
    {
        if(!Alive()) return Fail("send-deadline");
        ++SendCalls; result=send(SocketValue,(const char*)bytes+offset,(int)(size-offset),0);
        if(result==SOCKET_ERROR) return SocketFail("send");
        if(result<=0) return Fail("zero-send");
        offset+=(DWORD)result; BytesSent+=(DWORD)result;
    }
    return offset==size ? 1 : Fail("send-attempt-limit");
}
static int ReadExact(unsigned char* bytes,DWORD size)
{
    DWORD offset=0,attempts=0; int result;
    while(offset<size && ++attempts<=1024)
    {
        if(!Alive()) return Fail("receive-deadline");
        ++ReceiveCalls; result=recv(SocketValue,(char*)bytes+offset,(int)(size-offset),0);
        if(result==SOCKET_ERROR) return SocketFail("receive");
        if(!result) { SawEof=1; return Fail("early-eof"); }
        offset+=(DWORD)result; BytesReceived+=(DWORD)result;
    }
    return offset==size ? 1 : Fail("receive-attempt-limit");
}
static int ReadFrame(DWORD* size)
{
    unsigned short payload;
    if(++FrameCount>64) return Fail("frame-limit");
    if(!ReadExact(ReceiveFrame,17)) return 0;
    payload=Read16(ReceiveFrame+15);
    if(payload>256) return Fail("oversize-frame");
    if(payload && !ReadExact(ReceiveFrame+17,payload)) return 0;
    *size=17+(DWORD)payload; return 1;
}
static int SendPacket(unsigned short type,DWORD field1,DWORD field2,const unsigned char* data,unsigned short size)
{
    if(size>256) return Fail("send-payload-bound");
    Put16(SendFrame,1); Put16(SendFrame+2,0); Put16(SendFrame+4,type); SendFrame[6]=1;
    Put32(SendFrame+7,field1); Put32(SendFrame+11,field2); Put16(SendFrame+15,size);
    if(size) memcpy(SendFrame+17,data,size);
    return SendExact(SendFrame,17+(DWORD)size);
}
static void Pattern(unsigned short size,unsigned role)
{ unsigned i; for(i=0;i<size;++i) PatternBytes[i]=(unsigned char)(size+i*17+role*31); }
static int ValidatePattern(const unsigned char* bytes,unsigned short size,unsigned role)
{ unsigned i; for(i=0;i<size;++i) if(bytes[i]!=(unsigned char)(size+i*17+role*31)) return 0; return 1; }
static int BeginApplication(void)
{
    unsigned i;
    Put32(PatternBytes,0x01020304UL); Put32(PatternBytes+4,0);
    if(!SendPacket(5,0,0,PatternBytes,8)) return 0;
    PingSent=1;
    for(i=0;i<3;++i)
    {
        Pattern(Sizes[i],1);
        if(!SendPacket(2,0x57434f52UL,Sizes[i],PatternBytes,Sizes[i])) return 0;
        ++RequestsSent;
    }
    return 1;
}
static int HandleFrame(DWORD frameSize)
{
    unsigned short type=Read16(ReceiveFrame+4),size=Read16(ReceiveFrame+15);
    DWORD classId=Read32(ReceiveFrame+7),key=Read32(ReceiveFrame+11); unsigned index;
    if(Read16(ReceiveFrame)!=0 || Read16(ReceiveFrame+2)!=1 || ReceiveFrame[6]!=1) return Fail("frame-identity");
    if(type==4)
    {
        if(GotStart || frameSize!=sizeof(Start) || !EqualBytes(ReceiveFrame,Start,sizeof(Start))) return Fail("game-start");
        GotStart=1; return BeginApplication();
    }
    if(type==5)
    {
        if(classId || key || size!=8 || Read32(ReceiveFrame+21)!=0) return Fail("hello-request");
        memcpy(PatternBytes,ReceiveFrame+17,4); Put32(PatternBytes+4,GetTickCount());
        ++HostHelloRequests;
        if(!SendPacket(6,0,0,PatternBytes,8)) return 0;
        ++HostHelloReplies; return 1;
    }
    if(type==6)
    {
        if(!GotStart || !PingSent || GotPong || classId || key || size!=8 || Read32(ReceiveFrame+17)!=0x01020304UL) return Fail("hello-reply");
        PongSecondWord=Read32(ReceiveFrame+21); GotPong=1; return 1;
    }
    if(type!=2 || !GotStart || key>256 || size!=key) return Fail("application-frame");
    for(index=0;index<3 && Sizes[index]!=size;++index) {}
    if(index==3) return Fail("application-size");
    if(classId==0x57434f50UL)
    {
        if((HostRequestMask&(1UL<<index)) || !ValidatePattern(ReceiveFrame+17,size,0)) return Fail("host-request");
        HostRequestMask|=1UL<<index; ++RequestsReceived;
        if(!SendPacket(2,0x57434f51UL,key,ReceiveFrame+17,size)) return 0;
        ++RepliesSent; return 1;
    }
    if(classId==0x57434f53UL)
    {
        if((OwnReplyMask&(1UL<<index)) || !ValidatePattern(ReceiveFrame+17,size,1)) return Fail("guest-reply");
        OwnReplyMask|=1UL<<index; ++RepliesReceived; return 1;
    }
    return Fail("application-class");
}
static int ConnectSocket(void)
{
    u_long nonblocking=1; DWORD connectAt,elapsed; int result,error,length;
    BOOL noDelay=TRUE; DWORD timeout=5000; struct linger noLinger={0,0};
    struct timeval wait; fd_set writable,exceptional;
    SocketValue=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(SocketValue==INVALID_SOCKET) return SocketFail("socket");
    ++SocketsOpened;
    if(ioctlsocket(SocketValue,FIONBIO,&nonblocking)==SOCKET_ERROR) return SocketFail("nonblocking");
    if(setsockopt(SocketValue,SOL_SOCKET,SO_LINGER,(const char*)&noLinger,sizeof(noLinger))==SOCKET_ERROR ||
       setsockopt(SocketValue,IPPROTO_TCP,TCP_NODELAY,(const char*)&noDelay,sizeof(noDelay))==SOCKET_ERROR)
        return SocketFail("socket-options");
    result=connect(SocketValue,(const struct sockaddr*)&Server,sizeof(Server));
    if(result==SOCKET_ERROR)
    {
        error=WSAGetLastError();
        if(error!=WSAEWOULDBLOCK && error!=WSAEINPROGRESS && error!=WSAEALREADY) return SocketFail("connect");
        connectAt=GetTickCount();
        for(ConnectPolls=0;ConnectPolls<128;++ConnectPolls)
        {
            elapsed=(DWORD)(GetTickCount()-connectAt);
            if(elapsed>=5000 || !Alive()) return Fail("connect-deadline");
            FD_ZERO(&writable); FD_ZERO(&exceptional); FD_SET(SocketValue,&writable); FD_SET(SocketValue,&exceptional);
            wait.tv_sec=0; wait.tv_usec=100000;
            result=select(0,0,&writable,&exceptional,&wait);
            if(result==SOCKET_ERROR) return SocketFail("connect-select");
            if(result)
            {
                error=0; length=sizeof(error);
                if(getsockopt(SocketValue,SOL_SOCKET,SO_ERROR,(char*)&error,&length)==SOCKET_ERROR) return SocketFail("connect-error-query");
                if(error) { LastSocketError=error; if(!FirstSocketError) FirstSocketError=error; return Fail("connect-completion"); }
                break;
            }
        }
        if(ConnectPolls>=128) return Fail("connect-attempt-limit");
    }
    nonblocking=0;
    if(ioctlsocket(SocketValue,FIONBIO,&nonblocking)==SOCKET_ERROR ||
       setsockopt(SocketValue,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout))==SOCKET_ERROR ||
       setsockopt(SocketValue,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout))==SOCKET_ERROR)
        return SocketFail("blocking-timeouts");
    length=sizeof(Local);
    if(getsockname(SocketValue,(struct sockaddr*)&Local,&length)==SOCKET_ERROR) return SocketFail("local-endpoint");
    length=sizeof(Remote);
    if(getpeername(SocketValue,(struct sockaddr*)&Remote,&length)==SOCKET_ERROR) return SocketFail("remote-endpoint");
    return 1;
}
static int Exchange(void)
{
    DWORD frameSize,attempts; int result;
    if(!ConnectSocket() || !SendExact(Probe,sizeof(Probe)) || !ReadFrame(&frameSize)) return 0;
    if(frameSize!=sizeof(Info) || !EqualBytes(ReceiveFrame,Info,sizeof(Info))) return Fail("initial-info");
    if(!SendExact(Ready,sizeof(Ready))) return 0;
    while(Alive() && !(GotStart && GotPong && OwnReplyMask==7 && HostRequestMask==7))
    { if(!ReadFrame(&frameSize) || !HandleFrame(frameSize)) return 0; }
    if(!Alive() || RequestsSent!=3 || RequestsReceived!=3 || RepliesSent!=3 || RepliesReceived!=3 || !GotPong)
        return Fail("exchange-incomplete");
    if(shutdown(SocketValue,SD_SEND)==SOCKET_ERROR) return SocketFail("shutdown-send");
    for(attempts=0;attempts<32 && Alive();++attempts)
    {
        ++ReceiveCalls; result=recv(SocketValue,(char*)ReceiveFrame,sizeof(ReceiveFrame),0);
        if(result==SOCKET_ERROR) return SocketFail("eof-drain");
        if(!result) { SawEof=1; return 1; }
        BytesReceived+=(DWORD)result;
        return Fail("unexpected-after-shutdown");
    }
    return Fail("eof-deadline");
}
static void CloseNetwork(void)
{
    if(SocketValue!=INVALID_SOCKET)
    {
        if(closesocket(SocketValue)==SOCKET_ERROR)
        { CloseError=WSAGetLastError(); if(!FirstSocketError) FirstSocketError=CloseError; LastSocketError=CloseError; Fail("close-socket"); }
        else { SocketValue=INVALID_SOCKET; ++SocketsClosed; }
    }
    if(WsaStarts)
    {
        ++WsaCleanups;
        if(WSACleanup()==SOCKET_ERROR)
        { CleanupError=WSAGetLastError(); if(!FirstSocketError) FirstSocketError=CleanupError; LastSocketError=CleanupError; Fail("winsock-cleanup"); }
    }
}
static void Append(const char* value)
{ while(*value && JsonSize+1<sizeof(Json)) Json[JsonSize++]=*value++; if(*value) FileError=ERROR_INSUFFICIENT_BUFFER; }
static void UInt(DWORD value)
{
    char digits[10]; unsigned size=0;
    do { digits[size++]=(char)('0'+value%10); value/=10; } while(value && size<sizeof(digits));
    while(size) { char digit[2]; digit[0]=digits[--size]; digit[1]=0; Append(digit); }
}
static void Field(const char* name,DWORD value)
{ Append(",\""); Append(name); Append("\":"); UInt(value); }
static void AddressJson(const struct sockaddr_in* address)
{
    DWORD ip=ntohl(address->sin_addr.s_addr);
    Append("{\"address\":\""); UInt(ip>>24); Append("."); UInt((ip>>16)&255); Append("."); UInt((ip>>8)&255); Append("."); UInt(ip&255);
    Append("\",\"port\":"); UInt(ntohs(address->sin_port)); Append("}");
}
static int WriteReport(void)
{
    HANDLE file; DWORD written=0; BOOL success;
    Append("{\"kind\":\"XP32-independent-socket-oracle\",\"passed\":"); Append(Passed?"true":"false");
    Append(",\"productionPort\":false,\"failure\":\""); Append(Failure); Append("\"");
    Field("pid",GetCurrentProcessId()); Field("pointerBits",32); Field("elapsedMs",(DWORD)(GetTickCount()-Began));
    Append(",\"sidecarUsed\":"); Append(SidecarUsed?"true":"false"); Field("configurationError",ConfigurationError);
    Append(",\"osVersion\":{\"major\":"); UInt(Version.dwMajorVersion); Field("minor",Version.dwMinorVersion); Field("build",Version.dwBuildNumber);
    Field("platform",Version.dwPlatformId); Field("servicePackMajor",Version.wServicePackMajor); Field("servicePackMinor",Version.wServicePackMinor);
    Append(",\"querySucceeded\":"); Append(VersionOk?"true":"false"); Append("}");
    Append(",\"isWindowsXP\":"); Append(VersionOk && Version.dwMajorVersion==5 && Version.dwMinorVersion==1?"true":"false");
    Append(",\"local\":"); AddressJson(&Local); Append(",\"remote\":"); AddressJson(&Remote);
    Field("requestsSent",RequestsSent); Field("requestsReceived",RequestsReceived); Field("repliesSent",RepliesSent); Field("repliesReceived",RepliesReceived);
    Field("hostHelloRequests",HostHelloRequests); Field("hostHelloReplies",HostHelloReplies);
    Append(",\"pong\":"); Append(GotPong?"true":"false"); Field("pongEcho",GotPong?0x01020304UL:0); Field("pongServerStamp",PongSecondWord);
    Append(",\"eof\":"); Append(SawEof?"true":"false"); Field("sendCalls",SendCalls); Field("receiveCalls",ReceiveCalls);
    Field("connectPolls",ConnectPolls); Field("framesReceived",FrameCount);
    Field("bytesSent",BytesSent); Field("bytesReceived",BytesReceived); Field("wsaStarts",WsaStarts); Field("wsaCleanups",WsaCleanups);
    Field("socketsOpened",SocketsOpened); Field("socketsClosed",SocketsClosed); Field("firstSocketError",(DWORD)FirstSocketError);
    Field("lastSocketError",(DWORD)LastSocketError); Field("startupError",(DWORD)StartupError); Field("closeError",(DWORD)CloseError); Field("cleanupError",(DWORD)CleanupError); Field("versionError",VersionError);
    Append("}\r\n");
    if(FileError || !HaveReport) return 0;
    file=CreateFileA(ReportPath,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE) { FileError=GetLastError(); return 0; }
    success=WriteFile(file,Json,JsonSize,&written,0);
    if(!success) FileError=GetLastError();
    CloseHandle(file);
    return success && written==JsonSize;
}

__declspec(noreturn) void __cdecl ProbeEntry(void)
{
    int network=0;
    Began=GetTickCount(); Version.dwOSVersionInfoSize=sizeof(Version);
    VersionOk=GetVersionExA((OSVERSIONINFOA*)&Version)!=0;
    if(!VersionOk) VersionError=GetLastError();
    if(Arguments())
    {
        StartupError=WSAStartup(MAKEWORD(2,2),&Wsa);
        if(StartupError) Fail("winsock-startup");
        else
        {
            ++WsaStarts;
            if(Wsa.wVersion!=MAKEWORD(2,2)) Fail("winsock-version");
            else network=Exchange();
        }
    }
    CloseNetwork();
    Passed=network && VersionOk && SawEof && !StartupError && !CloseError && !CleanupError &&
        WsaStarts==1 && WsaCleanups==1 && SocketsOpened==1 && SocketsClosed==1 && EqualText(Failure,"none");
    if(!WriteReport()) ExitProcess(3);
    ExitProcess(Passed?0:3);
}
