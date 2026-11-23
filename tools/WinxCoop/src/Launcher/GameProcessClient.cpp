#include "GameProcessClient.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace winxcoop::launcher
{
    using namespace adapter;
    namespace
    {
        constexpr ULONGLONG CommandDeadline=10000;
        constexpr std::size_t ReportLimit=65536;
        struct Handle final
        {
            HANDLE value{};
            ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
            bool Valid() const noexcept {return value && value!=INVALID_HANDLE_VALUE;}
        };
        void Require(const bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
        [[noreturn]] void WinError(const char* message)
        {throw std::runtime_error(std::string(message)+" (win32="+std::to_string(GetLastError())+')');}
        std::uint64_t ProcessCreation(HANDLE process)
        {
            FILETIME created{},exited{},kernel{},user{};
            if(!GetProcessTimes(process,&created,&exited,&kernel,&user))WinError("Owned process creation time unavailable");
            const auto value=(std::uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
            Require(value!=0,"Owned process creation identity is empty");return value;
        }
        bool Same(const void* a,const void* b,const std::size_t bytes) noexcept {return std::memcmp(a,b,bytes)==0;}
        std::wstring Absolute(const std::wstring& value,const std::size_t limit)
        {
            Require(value.size()>3 && value.size()<limit && value[1]==L':' && (value[2]==L'\\' || value[2]==L'/'),"Absolute ASCII drive path required");
            for(const auto c:value)Require(c>=32 && c<=126 && c!=L'"',"Plain ASCII path without quotes required");
            wchar_t output[MAX_PATH]{};const DWORD count=GetFullPathNameW(value.c_str(),MAX_PATH,output,nullptr);
            Require(count>3 && count<limit,"Path exceeds bounded client contract");
            std::wstring result(output);while(result.size()>3 && (result.back()==L'\\' || result.back()==L'/'))result.pop_back();return result;
        }
        std::string Narrow(const std::wstring& value)
        {std::string result;for(const auto c:value){Require(c>=32 && c<=126,"ASCII text required");result+=static_cast<char>(c);}return result;}
        std::wstring Quote(const std::wstring& value)
        {
            std::wstring result=L"\"";std::size_t slashes{};
            for(const auto c:value)
            {
                if(c==L'\\'){++slashes;continue;}
                result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
            }
            result.append(slashes*2,L'\\');return result+L'"';
        }
        bool SamePath(std::string a,std::string b)
        {
            for(auto* text:{&a,&b})
            {
                for(auto& c:*text){if(c=='/')c='\\';if(c>='A' && c<='Z')c+=32;}
                while(text->size()>3 && text->back()=='\\')text->pop_back();
            }
            return a==b;
        }
        std::string ReadFileBounded(const std::wstring& path,const std::size_t maximum)
        {
            Handle file;file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!file.Valid())WinError("Completed report unavailable");LARGE_INTEGER size{};
            Require(GetFileSizeEx(file.value,&size) && size.QuadPart>0 && static_cast<ULONGLONG>(size.QuadPart)<=maximum,"Completed report has an invalid size");
            std::string result(static_cast<std::size_t>(size.QuadPart),'\0');DWORD got{};
            if(!ReadFile(file.value,result.data(),static_cast<DWORD>(result.size()),&got,nullptr) || got!=result.size())WinError("Completed report read failed");
            return result;
        }
        void WriteFreshAscii(const std::wstring& path,const std::wstring& value)
        {
            const auto bytes=Narrow(value);Require(!bytes.empty() && bytes.size()<=4096,"Host sidecar exceeds 4096 ASCII bytes");
            Handle file;file.value=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!file.Valid())WinError("Fresh host sidecar unavailable");DWORD written{};
            if(!WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) || written!=bytes.size() || !FlushFileBuffers(file.value))
                WinError("Host sidecar write failed");
        }
        void VerifyCopy(HANDLE source,HANDLE copy)
        {
            LARGE_INTEGER a{},b{},zero{};
            Require(GetFileSizeEx(source,&a) && GetFileSizeEx(copy,&b) && a.QuadPart>0 && a.QuadPart==b.QuadPart && a.QuadPart<=64*1024*1024,"Copied helper size differs");
            if(!SetFilePointerEx(source,zero,nullptr,FILE_BEGIN) || !SetFilePointerEx(copy,zero,nullptr,FILE_BEGIN))WinError("Helper copy rewind failed");
            std::array<BYTE,4096> left{},right{};
            for(;;)
            {
                DWORD x{},y{};if(!ReadFile(source,left.data(),static_cast<DWORD>(left.size()),&x,nullptr) ||
                    !ReadFile(copy,right.data(),static_cast<DWORD>(right.size()),&y,nullptr))WinError("Helper copy comparison failed");
                Require(x==y && Same(left.data(),right.data(),x),"Copied helper bytes differ");if(!x)break;
            }
        }
        // Full bounded JSON parsing, including unknown fields, duplicate keys,
        // escapes and the final EOF. No report is accepted by substring search.
        struct Json final
        {
            enum class Kind { Null, Boolean, Number, String, Object, Array } kind{};
            bool boolean{};std::string text;std::vector<std::string> keys;std::vector<Json> children;
            const Json& At(const char* key) const
            {
                Require(kind==Kind::Object,"Report field is not an object");const auto found=std::find(keys.begin(),keys.end(),key);
                if(found==keys.end())throw std::runtime_error(std::string("Required completed-report field is missing: ")+key);
                return children[static_cast<std::size_t>(found-keys.begin())];
            }
            bool Boolean() const {Require(kind==Kind::Boolean,"Report field is not a boolean");return boolean;}
            const std::string& String() const {Require(kind==Kind::String,"Report field is not a string");return text;}
            std::uint64_t Unsigned(const std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max()) const
            {
                Require(kind==Kind::Number && !text.empty(),"Report field is not an unsigned integer");std::uint64_t value{};
                for(const char c:text)
                {Require(c>='0' && c<='9',"Report integer has a sign or fraction");const auto digit=static_cast<unsigned>(c-'0');
                    Require(digit<=maximum && value<=(maximum-digit)/10,"Report integer overflows its field");value=value*10+digit;}
                return value;
            }
        };
        class JsonParser final
        {
        public:
            explicit JsonParser(const std::string& text):text_(text){Require(!text.empty() && text.size()<=ReportLimit,"Report exceeds JSON byte bound");}
            Json Parse(){Json result=Value(0);Space();Require(at_==text_.size(),"Completed report contains trailing or partial JSON");return result;}
        private:
            const std::string& text_;std::size_t at_{},nodes_{};
            char Peek() const noexcept {return at_<text_.size()?text_[at_]:'\0';}
            char Take(){Require(at_<text_.size(),"Truncated completed JSON report");return text_[at_++];}
            void Space(){while(Peek()==' ' || Peek()=='\r' || Peek()=='\n' || Peek()=='\t')++at_;}
            void Literal(const char* value){while(*value)Require(Take()==*value++,"Invalid JSON literal");}
            unsigned Hex(){const char c=Take();if(c>='0' && c<='9')return c-'0';if(c>='a' && c<='f')return c-'a'+10;if(c>='A' && c<='F')return c-'A'+10;throw std::runtime_error("Invalid JSON Unicode escape");}
            unsigned Codepoint(){unsigned result{};for(unsigned i=0;i<4;++i)result=(result<<4)|Hex();return result;}
            static void Utf8(std::string& value,const unsigned point)
            {
                if(point<0x80)value+=static_cast<char>(point);
                else if(point<0x800){value+=static_cast<char>(0xC0|(point>>6));value+=static_cast<char>(0x80|(point&63));}
                else if(point<0x10000){value+=static_cast<char>(0xE0|(point>>12));value+=static_cast<char>(0x80|((point>>6)&63));value+=static_cast<char>(0x80|(point&63));}
                else{value+=static_cast<char>(0xF0|(point>>18));value+=static_cast<char>(0x80|((point>>12)&63));value+=static_cast<char>(0x80|((point>>6)&63));value+=static_cast<char>(0x80|(point&63));}
            }
            std::string String()
            {
                Require(Take()=='"',"Expected JSON string");std::string value;
                for(;;)
                {
                    const auto c=static_cast<unsigned char>(Take());if(c=='"')return value;Require(c>=32,"Unescaped control byte in JSON string");
                    if(c=='\\')
                    {
                        const char escaped=Take();
                        if(escaped=='"' || escaped=='\\' || escaped=='/')value+=escaped;
                        else if(escaped=='b')value+='\b';else if(escaped=='f')value+='\f';else if(escaped=='n')value+='\n';else if(escaped=='r')value+='\r';else if(escaped=='t')value+='\t';
                        else if(escaped=='u')
                        {
                            unsigned point=Codepoint();
                            if(point>=0xD800 && point<=0xDBFF)
                            {Require(Take()=='\\' && Take()=='u',"Missing low JSON surrogate");const unsigned low=Codepoint();Require(low>=0xDC00 && low<=0xDFFF,"Invalid low JSON surrogate");point=0x10000+((point-0xD800)<<10)+(low-0xDC00);}
                            else Require(point<0xDC00 || point>0xDFFF,"Unpaired low JSON surrogate");
                            Utf8(value,point);
                        }
                        else throw std::runtime_error("Invalid JSON escape");
                    }
                    else if(c<128)value+=static_cast<char>(c);
                    else
                    {
                        const unsigned count=c>=0xC2 && c<=0xDF?1:c>=0xE0 && c<=0xEF?2:c>=0xF0 && c<=0xF4?3:0;
                        Require(count!=0,"Invalid UTF8 leading byte");unsigned point=c&((1u<<(6-count))-1);
                        for(unsigned i=0;i<count;++i){const auto next=static_cast<unsigned char>(Take());Require((next&0xC0)==0x80,"Invalid UTF8 continuation");point=(point<<6)|(next&63);}
                        Require(point>=(count==1?0x80u:count==2?0x800u:0x10000u) && point<=0x10FFFF && (point<0xD800 || point>0xDFFF),"Noncanonical UTF8 report string");Utf8(value,point);
                    }
                    Require(value.size()<=4096,"Report JSON string exceeds bound");
                }
            }
            Json Value(const unsigned depth)
            {
                Require(depth<=16 && ++nodes_<=4096,"Completed JSON report exceeds structural bounds");Space();Json result;const char first=Peek();
                if(first=='n'){Literal("null");return result;}
                if(first=='t' || first=='f'){result.kind=Json::Kind::Boolean;result.boolean=first=='t';Literal(first=='t'?"true":"false");return result;}
                if(first=='"'){result.kind=Json::Kind::String;result.text=String();return result;}
                if(first=='{' || first=='[')
                {
                    const bool object=Take()=='{';result.kind=object?Json::Kind::Object:Json::Kind::Array;Space();const char end=object?'}':']';
                    if(Peek()==end){Take();return result;}
                    for(;;)
                    {
                        Space();if(object){auto key=String();Require(std::find(result.keys.begin(),result.keys.end(),key)==result.keys.end(),"Duplicate JSON report key");result.keys.push_back(std::move(key));Space();Require(Take()==':',"Missing JSON object colon");}
                        result.children.push_back(Value(depth+1));Space();const char delimiter=Take();if(delimiter==end)return result;Require(delimiter==',',"Invalid JSON container delimiter");
                    }
                }
                result.kind=Json::Kind::Number;const auto begin=at_;if(Peek()=='-')Take();
                if(Peek()=='0')Take();else{Require(Peek()>='1' && Peek()<='9',"Invalid JSON number");while(Peek()>='0' && Peek()<='9')Take();}
                if(Peek()=='.'){Take();Require(Peek()>='0' && Peek()<='9',"Truncated JSON fraction");while(Peek()>='0' && Peek()<='9')Take();}
                if(Peek()=='e' || Peek()=='E'){Take();if(Peek()=='+' || Peek()=='-')Take();Require(Peek()>='0' && Peek()<='9',"Truncated JSON exponent");while(Peek()>='0' && Peek()<='9')Take();}
                result.text=text_.substr(begin,at_-begin);return result;
            }
        };
        Json Report(const std::wstring& path){const auto bytes=ReadFileBounded(path,ReportLimit);return JsonParser(bytes).Parse();}
    }

    struct GameProcessClient::Impl final
    {
        GameProcessClientConfiguration config;
        GameProcessClientIdentity identity;
        GameProcessClientResult result;
        GameProcessClientPhase phase=GameProcessClientPhase::Idle;
        DWORD ownerThread=GetCurrentThreadId();
        Handle runtimeMapping,statusMapping,stopEvent,outerProcess,caseHandle,hostSource,hostCopy,outerFile,mediaLease;
        RuntimeShared* runtime{};
        GameHostStatusShared* status{};
        RuntimeSnapshot snapshot{};
        RuntimeAck acknowledgement{};
        RuntimeCommand pending{},lastPublished{};
        GameHostStatusSnapshot helper{};
        std::wstring runtimeName,statusName,stopName,caseHost,hostReport,outerReport;
        std::string failure;
        ULONGLONG began{},commandBegan{},commandLastCheck{},commandReadyWait{};
        std::uint32_t logicalEntity{}, followSourceGeneration{}, followTargetLevel{};
        bool attempted{},stopRequested{},haveSnapshot{},haveHelper{},pendingActive{},finalized{};

        explicit Impl(GameProcessClientConfiguration value):config(std::move(value)){result.directHost=config.menuFirst;}
        bool OwnerProcessStarted() const noexcept {return result.outerStarted || result.helperStarted;}
        ~Impl()
        {
            if(stopEvent.Valid() && !finalized)SetEvent(stopEvent.value);
            if(runtime)UnmapViewOfFile(runtime);if(status)UnmapViewOfFile(status);
        }
        void OwnerThread() const {Require(GetCurrentThreadId()==ownerThread,"GameProcessClient requires its GUI owner thread");}
        void Stop() noexcept
        {
            stopRequested=true;if(stopEvent.Valid())SetEvent(stopEvent.value);
            if(!finalized && OwnerProcessStarted())phase=GameProcessClientPhase::Stopping;
        }
        void Fail(const std::string& message)
        {
            if(failure.empty())
            {
                failure=message;
                try
                {
                    // Observation only: preserve the first failure and do not
                    // accept a late ACK or change the command/stop ordering.
                    const auto now=GetTickCount64();RuntimeAck rawAck{};
                    const bool ackRead=runtime && TryRead(runtime->acknowledgement,rawAck);
                    std::string context=" [clientContext";
                    const auto number=[&context](const char* name,const std::uint64_t value)
                    {context+=' ';context+=name;context+='=';context+=std::to_string(value);};
                    number("nowMs",now);number("pending",pendingActive);number("pendingSeq",pending.sequence);
                    number("pendingOp",static_cast<unsigned>(pending.operation));number("commandBeganMs",commandBegan);
                    number("commandReadyWaitMs",commandReadyWait);number("nativePresent",haveSnapshot);
                    number("nativeMs",snapshot.monotonicMilliseconds);number("nativeWorld",snapshot.worldUpdates);
                    number("nativeLastSeq",snapshot.lastCommandSequence);number("nativePhase",static_cast<unsigned>(snapshot.phase));
                    number("helperPresent",haveHelper);number("helperMs",helper.monotonicMilliseconds);
                    number("helperPhase",static_cast<unsigned>(helper.phase));number("rawAckRead",ackRead);
                    if(ackRead)
                    {
                        number("rawAckSeq",rawAck.sequence);number("rawAckStatus",static_cast<unsigned>(rawAck.status));
                        number("rawAckOp",static_cast<unsigned>(rawAck.operation));number("rawAckWorld",rawAck.worldUpdates);
                    }
                    context+=']';failure+=context;
                }
                catch(...){} // Diagnostics must not replace the error or prevent Stop.
            }
            result.error=failure;Stop();
        }
        void FileExists(const std::wstring& path,const bool directory)
        {
            const DWORD attributes=GetFileAttributesW(path.c_str());
            Require(attributes!=INVALID_FILE_ATTRIBUTES && bool(attributes&FILE_ATTRIBUTE_DIRECTORY)==directory,"Installed game/helper file or media directory is missing");
        }
        void Validate()
        {
            OwnerThread();config.game=Absolute(config.game,MAX_PATH);config.media=Absolute(config.media,MAX_PATH-40);
            config.adapter=Absolute(config.adapter,MAX_PATH);config.gameHost=Absolute(config.gameHost,MAX_PATH);
            if(!config.menuFirst)config.outer=Absolute(config.outer,MAX_PATH);
            config.caseDirectory=Absolute(config.caseDirectory,MAX_PATH-40);
            Require(config.session && config.levelEpoch && (config.menuFirst?config.seconds==0:config.seconds>=5 && config.seconds<=3600),"Nonzero session/epoch and seconds0 for menu-first or5..3600 for legacy diagnostic required");
            Require(config.startupLevel<=50 && config.startupEntry==0 && (!config.startupLevel || config.menuFirst),
                "Direct startup requires level1..50, default entry0 and the unbounded direct helper");
            Require(!config.sceneFollow || config.menuFirst,"Scene following is available only for the ordinary unbounded game path");
            for(const auto* file:{&config.game,&config.adapter,&config.gameHost})FileExists(*file,false);
            if(!config.menuFirst)FileExists(config.outer,false);FileExists(config.media,true);
        }
        void PrepareCase()
        {
            if(!CreateDirectoryW(config.caseDirectory.c_str(),nullptr))WinError("A fresh case directory is required");
            caseHandle.value=CreateFileW(config.caseDirectory.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
            if(!caseHandle.Valid())WinError("New case directory cannot be pinned");
            BY_HANDLE_FILE_INFORMATION info{};
            Require(GetFileInformationByHandle(caseHandle.value,&info) && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"New case directory is not an ordinary directory");
            caseHost=config.caseDirectory+L"\\WinxCoopGameHost.exe";hostReport=config.caseDirectory+L"\\host-report.json";outerReport=config.caseDirectory+L"\\outer-report.json";
            hostSource.value=CreateFileW(config.gameHost.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!config.menuFirst)outerFile.value=CreateFileW(config.outer.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!hostSource.Valid() || (!config.menuFirst && !outerFile.Valid()))WinError("GameHost/outer program pin failed");
            if(!CopyFileW(config.gameHost.c_str(),caseHost.c_str(),TRUE))WinError("Fresh per-case GameHost copy failed");
            hostCopy.value=CreateFileW(caseHost.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!hostCopy.Valid())WinError("Copied GameHost cannot be pinned");VerifyCopy(hostSource.value,hostCopy.value);
        }
        void AcquireMediaLease()
        {
            SetLastError(ERROR_SUCCESS);const HANDLE created=CreateEventW(nullptr,TRUE,FALSE,L"Local\\WinxCoop.MediaPathLease.v1");
            const DWORD error=GetLastError();if(!created)WinError("MediaPath lease unavailable");
            if(error==ERROR_ALREADY_EXISTS){CloseHandle(created);throw std::runtime_error("Another owned game is preparing or restoring MediaPath");}
            mediaLease.value=created;result.mediaLeaseAcquired=true;
        }
        bool ReleaseMediaLease() noexcept
        {
            if(mediaLease.Valid())
            {
                if(!CloseHandle(mediaLease.value))return false;
                mediaLease.value=nullptr;result.mediaLeaseReleased=true;
            }
            return true;
        }
        void PrepareMappings()
        {
            identity.ownerPid=GetCurrentProcessId();identity.session=config.session;identity.levelEpoch=config.levelEpoch;
            FILETIME created{},exit{},kernel{},user{};
            if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))WinError("GUI process creation time unavailable");
            identity.ownerCreationTime=(std::uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
            Require(identity.ownerCreationTime && BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&identity.nonce),sizeof(identity.nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0 && identity.nonce,"Fresh client nonce unavailable");
            const auto prefix=L"Local\\WinxCoop.Client."+std::to_wstring(identity.ownerPid)+L'.'+std::to_wstring(identity.nonce);
            runtimeName=prefix+L".Runtime";statusName=prefix+L".Status";stopName=prefix+L".Stop";
            Require(runtimeName.size()<128 && statusName.size()<128 && stopName.size()<128,"Client object name exceeds service contract");
            SetLastError(ERROR_SUCCESS);runtimeMapping.value=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(RuntimeShared),runtimeName.c_str());
            if(!runtimeMapping.Valid())WinError("Client runtime mapping failed");Require(GetLastError()!=ERROR_ALREADY_EXISTS,"Client runtime mapping collision");
            SetLastError(ERROR_SUCCESS);statusMapping.value=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(GameHostStatusShared),statusName.c_str());
            if(!statusMapping.Valid())WinError("Client helper-status mapping failed");Require(GetLastError()!=ERROR_ALREADY_EXISTS,"Client helper-status mapping collision");
            SetLastError(ERROR_SUCCESS);stopEvent.value=CreateEventW(nullptr,TRUE,FALSE,stopName.c_str());
            if(!stopEvent.Valid())WinError("Client stop event failed");Require(GetLastError()!=ERROR_ALREADY_EXISTS,"Client stop-event collision");
            runtime=static_cast<RuntimeShared*>(MapViewOfFile(runtimeMapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(RuntimeShared)));
            status=static_cast<GameHostStatusShared*>(MapViewOfFile(statusMapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(GameHostStatusShared)));
            Require(runtime && status,"Client mapping views unavailable");ZeroMemory(runtime,sizeof(*runtime));ZeroMemory(status,sizeof(*status));
            runtime->magic=IpcMagic;runtime->version=IpcVersion;runtime->bytes=sizeof(*runtime);runtime->launcherPid=identity.ownerPid;runtime->nonce=identity.nonce;
            status->magic=GameHostStatusMagic;status->version=GameHostStatusVersion;status->bytes=sizeof(*status);status->ownerPid=identity.ownerPid;
            status->nonce=identity.nonce;status->ownerCreationTime=identity.ownerCreationTime;
            ZeroMemory(&lastPublished,sizeof(lastPublished));ZeroMemory(&acknowledgement,sizeof(acknowledgement));
        }
        void Start()
        {
            OwnerThread();Require(!attempted,"GameProcessClient generation cannot be reused");attempted=true;Validate();
            if(!config.menuFirst)AcquireMediaLease();PrepareCase();PrepareMappings();
            const auto argument=[](const wchar_t* name,const std::wstring& value){return std::wstring(L" ")+name+L' '+Quote(value);};
            const auto args=argument(L"--game",config.game)+argument(L"--adapter",config.adapter)+argument(L"--report",hostReport)+
                argument(L"--seconds",std::to_wstring(config.seconds))+argument(L"--menu-first",config.menuFirst?L"1":L"0")+argument(L"--media",config.media)+argument(L"--mapping",runtimeName)+
                argument(L"--status",statusName)+argument(L"--stop-event",stopName)+argument(L"--owner",std::to_wstring(identity.ownerPid))+
                argument(L"--owner-created",std::to_wstring(identity.ownerCreationTime))+argument(L"--nonce",std::to_wstring(identity.nonce))+
                argument(L"--session",std::to_wstring(config.session))+argument(L"--epoch",std::to_wstring(config.levelEpoch))+
                argument(L"--startup-level",std::to_wstring(config.startupLevel))+argument(L"--startup-entry",std::to_wstring(config.startupEntry))+
                argument(L"--scene-follow",config.sceneFollow?L"1":L"0");
            WriteFreshAscii(caseHost+L".args",args);
            if(config.menuFirst)
            {
                std::wstring command=Quote(caseHost)+args;Require(command.size()<4096,"Direct GameHost command exceeds its bound");
                STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
                PROCESS_INFORMATION process{};
                if(!CreateProcessW(caseHost.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP|CREATE_DEFAULT_ERROR_MODE,
                    nullptr,config.caseDirectory.c_str(),&startup,&process))WinError("Direct owned GameHost launch failed");
                outerProcess.value=process.hProcess;CloseHandle(process.hThread);identity.helperPid=process.dwProcessId;
                result.helperStarted=true;began=GetTickCount64();phase=GameProcessClientPhase::Starting;
                identity.helperCreationTime=ProcessCreation(outerProcess.value);VerifyDirectHost(true);return;
            }
            std::wstring command=Quote(config.outer)+argument(L"--game",caseHost)+argument(L"--media",config.media)+argument(L"--report",outerReport)+
                argument(L"--observe-ms",std::to_wstring((config.seconds+35)*1000));
            Require(command.size()<1024,"Outer command exceeds its ASCII input contract");
            Handle inheritedLease;
            if(!DuplicateHandle(GetCurrentProcess(),mediaLease.value,GetCurrentProcess(),&inheritedLease.value,0,TRUE,DUPLICATE_SAME_ACCESS))WinError("Inherited MediaPath lease duplication failed");
            SIZE_T attributeBytes{};InitializeProcThreadAttributeList(nullptr,1,0,&attributeBytes);
            Require(attributeBytes>0 && attributeBytes<=65536,"Invalid process attribute-list size");std::vector<BYTE> attributes(attributeBytes);
            auto* list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
            if(!InitializeProcThreadAttributeList(list,1,0,&attributeBytes))WinError("Process attribute-list initialization failed");
            struct DeleteAttributes {LPPROC_THREAD_ATTRIBUTE_LIST value;~DeleteAttributes(){DeleteProcThreadAttributeList(value);}} release{list};
            HANDLE inheritedHandles[]={inheritedLease.value};
            if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inheritedHandles,sizeof(inheritedHandles),nullptr,nullptr))WinError("Restricted inherited-handle list failed");
            STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;startup.StartupInfo.wShowWindow=SW_HIDE;
            startup.lpAttributeList=list;PROCESS_INFORMATION process{};
            if(!CreateProcessW(config.outer.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NEW_PROCESS_GROUP|CREATE_DEFAULT_ERROR_MODE,nullptr,config.caseDirectory.c_str(),&startup.StartupInfo,&process))WinError("Owned outer helper launch failed");
            outerProcess.value=process.hProcess;CloseHandle(process.hThread);identity.outerPid=process.dwProcessId;
            result.outerStarted=true;result.mediaLeaseInherited=true;began=GetTickCount64();phase=GameProcessClientPhase::Starting;
            if(!CloseHandle(inheritedLease.value))WinError("Temporary inherited lease handle close failed");inheritedLease.value=nullptr;
        }
        void VerifyDirectHost(const bool verifyImage=false) const
        {
            if(!config.menuFirst)return;
            Require(outerProcess.Valid() && identity.helperPid && identity.helperCreationTime &&
                GetProcessId(outerProcess.value)==identity.helperPid && ProcessCreation(outerProcess.value)==identity.helperCreationTime,
                "Direct GameHost process identity differs from the created instance");
            if(verifyImage)
            {
                wchar_t path[MAX_PATH]{};DWORD count=MAX_PATH;
                if(!QueryFullProcessImageNameW(outerProcess.value,0,path,&count))WinError("Direct GameHost image unavailable");
                Require(SamePath(Narrow(std::wstring(path,count)),Narrow(caseHost)),"Created direct GameHost image differs from its pinned case copy");
            }
        }
        void Headers() const
        {
            Require(runtime && status && runtime->magic==IpcMagic && runtime->version==IpcVersion && runtime->bytes==sizeof(*runtime) &&
                runtime->launcherPid==identity.ownerPid && runtime->nonce==identity.nonce && status->magic==GameHostStatusMagic &&
                status->version==GameHostStatusVersion && status->bytes==sizeof(*status) && status->ownerPid==identity.ownerPid &&
                status->nonce==identity.nonce && status->ownerCreationTime==identity.ownerCreationTime,"Client mapping identity changed");
        }
        void ReadHelper()
        {
            GameHostStatusSnapshot value{};if(!TryRead(status->snapshot,value) || value.phase==GameHostPhase::Empty)return;
            Require(static_cast<unsigned>(value.phase)<=static_cast<unsigned>(GameHostPhase::Failed) && value.helperPid &&
                value.helperPid!=identity.ownerPid && value.helperPid!=identity.outerPid,"Invalid GameHost status identity/phase");
            Require(!haveHelper || (value.helperPid==helper.helperPid && value.monotonicMilliseconds>=helper.monotonicMilliseconds),"GameHost status generation/counter regressed");
            Require(!identity.gamePid || value.gamePid==identity.gamePid,"GameHost changed the owned game PID");
            Require(!identity.mainThreadId || value.mainThreadId==identity.mainThreadId,"GameHost changed the owned main thread");
            if(identity.helperPid)Require(value.helperPid==identity.helperPid,"GameHost PID changed");
            identity.helperPid=value.helperPid;if(value.gamePid){identity.gamePid=value.gamePid;result.originalGameStarted=true;}if(value.mainThreadId)identity.mainThreadId=value.mainThreadId;
            helper=value;haveHelper=true;
            if(value.phase==GameHostPhase::Failed || value.error)
            {
                const auto end=std::find(std::begin(value.errorText),std::end(value.errorText),'\0');
                Require(end!=std::end(value.errorText),"Unterminated helper error text");
                throw std::runtime_error("GameHost failed: "+std::string(std::begin(value.errorText),end));
            }
            if(value.phase==GameHostPhase::Stopping || value.phase==GameHostPhase::Stopped)Stop();
        }
        void ReadSnapshot()
        {
            RuntimeSnapshot value{};if(!identity.gamePid || !identity.mainThreadId || !TryRead(runtime->snapshot,value) || value.phase==RuntimePhase::Empty)return;
            Require(value.gamePid==identity.gamePid && value.mainThreadId==identity.mainThreadId && value.mode==RuntimeMode::PassiveActor &&
                value.session==config.session && value.levelEpoch==config.levelEpoch && static_cast<unsigned>(value.phase)<=static_cast<unsigned>(RuntimePhase::Failed),"Native runtime snapshot identity/phase changed");
            Require(!haveSnapshot || (value.worldUpdates>=snapshot.worldUpdates && value.monotonicMilliseconds>=snapshot.monotonicMilliseconds &&
                value.lastCommandSequence>=snapshot.lastCommandSequence && value.sceneGeneration>=snapshot.sceneGeneration &&
                value.sceneRetirements>=snapshot.sceneRetirements),"Native runtime snapshot counter regressed");
            Require(value.sceneGeneration>=1 && value.sceneGeneration==value.sceneRetirements+(value.epochRetired?0u:1u) &&
                (config.sceneFollow || (value.sceneGeneration==1 && value.sceneRetirements<=1)),"Native scene generation is not qualified for this process");
            Require(value.lastCommandSequence<=result.commandsPublished,"Native runtime applied an unpublished command");
            if(haveSnapshot && (value.sceneRetirements>snapshot.sceneRetirements || value.sceneGeneration>snapshot.sceneGeneration))
                logicalEntity=0; // Actual native cleanup; an old Applied ACK cannot resurrect it below.
            snapshot=value;haveSnapshot=true;
            if(followSourceGeneration && value.sceneGeneration>followSourceGeneration && value.phase==RuntimePhase::Ready)
            {
                Require(value.levelId==followTargetLevel,"Follow request completed in an unassigned native scene");
                followSourceGeneration=followTargetLevel=0;
            }
            result.sceneGeneration=value.sceneGeneration;result.sceneRetirements=value.sceneRetirements;
            result.visualEpochRetired=value.epochRetired!=0;
            Require(value.phase!=RuntimePhase::Failed && value.error==RuntimeError::None && !value.actorError,"Native runtime failed its gate");
            if(value.epochRetired || value.phase==RuntimePhase::Stopped)
            {
                if(!config.menuFirst)Stop();
                else
                {
                    Require(value.epochRetired && value.phase==RuntimePhase::Stopped && !value.remoteEntity &&
                        value.remoteCreated==value.remoteRemoved,"Retired visual epoch has no confirmed passive cleanup");
                    // Keep the original game. Only an actual typed completion
                    // below may consume an outstanding immutable command.
                }
            }
        }
        bool NativeReadyForCommandProgress() const noexcept
        {
            const auto now=GetTickCount64();
            const bool fresh=!config.menuFirst || (now>=helper.monotonicMilliseconds && now-helper.monotonicMilliseconds<=1000 &&
                now>=snapshot.monotonicMilliseconds && now-snapshot.monotonicMilliseconds<=1000);
            return failure.empty() && !stopRequested && !finalized && haveHelper && helper.phase==GameHostPhase::Running &&
                haveSnapshot && snapshot.phase==RuntimePhase::Ready && !snapshot.epochRetired && snapshot.mediaQualified &&
                (config.menuFirst?snapshot.levelId>=1 && snapshot.levelId<=50:snapshot.levelId==28) && fresh && helper.fullContextRestored && helper.worldContextPreserved;
        }
        bool Ready() const noexcept {return !followSourceGeneration && NativeReadyForCommandProgress();}
        bool CanSubmitRemoval() const noexcept
        {
            if(pendingActive || !logicalEntity || !failure.empty() || stopRequested || finalized || !haveHelper || !haveSnapshot ||
                helper.phase!=GameHostPhase::Running || !helper.fullContextRestored || !helper.worldContextPreserved ||
                snapshot.error!=RuntimeError::None || snapshot.actorError || snapshot.epochRetired || result.visualEpochRetired ||
                !snapshot.mediaQualified || snapshot.remoteEntity!=logicalEntity ||
                std::uint64_t(snapshot.remoteCreated)!=std::uint64_t(snapshot.remoteRemoved)+1)return false;
            if(Ready())return true;
            if(!config.menuFirst || snapshot.phase!=RuntimePhase::WaitingForLevel)return false;
            const auto now=GetTickCount64();
            return now>=helper.monotonicMilliseconds && now-helper.monotonicMilliseconds<=1000 &&
                now>=snapshot.monotonicMilliseconds && now-snapshot.monotonicMilliseconds<=1000;
        }
        bool Matching(const RuntimeAck& value,const RuntimeCommand& command) const noexcept
        {
            return value.bytes==sizeof(value) && value.version==IpcVersion && value.session==command.session && value.levelEpoch==command.levelEpoch &&
                value.sequence==command.sequence && value.entity==command.entity && value.operation==command.operation &&
                value.sceneGeneration==command.sceneGeneration &&
                (value.status==RuntimeAckStatus::Applied || value.status==RuntimeAckStatus::CancelledOnRetirement) &&
                value.error==RuntimeError::None && !value.actorError;
        }
        void ReadAck()
        {
            RuntimeAck value{};if(!TryRead(runtime->acknowledgement,value))return;
            if(value.sequence==acknowledgement.sequence){Require(Same(&value,&acknowledgement,sizeof(value)),"Acknowledgement changed after completion");return;}
            Require(pendingActive && value.sequence==pending.sequence && Matching(value,pending),"Native acknowledgement is stale, future, failed or has wrong identity");
            if(value.status==RuntimeAckStatus::CancelledOnRetirement)
            {
                // The completion mailbox may precede the throttled snapshot.
                // Wait for actual cleanup evidence; do not relabel cancellation
                // as an Applied native operation or reset the shared command.
                if(!haveSnapshot || snapshot.sceneRetirements<value.sceneGeneration)return;
                Require(snapshot.sceneGeneration>=value.sceneGeneration,"Cancellation belongs to a future native scene");
            }
            Require(value.worldUpdates>0 && (!haveSnapshot || value.worldUpdates>=snapshot.worldUpdates || snapshot.lastCommandSequence>=value.sequence),"Native acknowledgement has an invalid world counter");
            CopyMemory(&acknowledgement,&value,sizeof(value));result.commandsCompleted=value.sequence;
            if(value.status==RuntimeAckStatus::CancelledOnRetirement)++result.commandsCancelledOnRetirement;
            else
            {
                ++result.commandsAcknowledged;
                if(haveSnapshot && !snapshot.epochRetired && snapshot.sceneGeneration==value.sceneGeneration)
                {
                    if(pending.operation==RuntimeOperation::Create)logicalEntity=pending.entity;
                    else if(pending.operation==RuntimeOperation::Remove)logicalEntity=0;
                }
            }
            pendingActive=false;pending={}; // Shared command remains immutable until the next sequence.
        }
        void PublishCommand(const RuntimeOperation operation,const std::uint32_t entity,const Transform& transform,
            const RuntimeAnimation animation,const std::uint32_t targetLevel=0,const std::uint32_t targetEntry=0)
        {
            Require(!pendingActive && result.commandsPublished==result.commandsCompleted &&
                std::uint64_t(result.commandsCompleted)==std::uint64_t(result.commandsAcknowledged)+result.commandsCancelledOnRetirement,
                "Native command outcomes do not account for the previous immutable sequence");
            Require(result.commandsPublished<MAXDWORD,"Native command sequence exhausted");RuntimeCommand previous{};
            Require(TryRead(runtime->command,previous) && Same(&previous,&lastPublished,sizeof(previous)),"Shared command changed outside this client");
            ZeroMemory(&pending,sizeof(pending));pending.bytes=sizeof(pending);pending.version=IpcVersion;pending.session=config.session;pending.levelEpoch=config.levelEpoch;
            pending.sceneGeneration=snapshot.sceneGeneration;
            pending.sequence=result.commandsPublished+1;pending.entity=entity;pending.operation=operation;pending.animation=animation;pending.remote=transform;
            pending.targetLevel=targetLevel;pending.targetEntry=targetEntry;
            if(operation==RuntimeOperation::FollowLevel){followSourceGeneration=snapshot.sceneGeneration;followTargetLevel=targetLevel;}
            pendingActive=true;commandBegan=GetTickCount64();commandLastCheck=commandBegan;commandReadyWait=0;result.commandsPublished=pending.sequence;
            CopyMemory(&lastPublished,&pending,sizeof(pending));Publish(runtime->command,pending);
        }
        bool Submit(const RuntimeOperation operation,const std::uint32_t entity,const Transform& transform,const RuntimeAnimation animation)
        {
            OwnerThread();Require(operation>=RuntimeOperation::Create && operation<=RuntimeOperation::Remove && entity>=1 && entity<=2 &&
                animation<=RuntimeAnimation::Jump,"Invalid local native operation/entity/animation");
            if(operation!=RuntimeOperation::Remove)
            {
                for(const auto value:transform.position)Require(std::isfinite(value),"Nonfinite local position");
                for(const auto value:transform.scale)Require(std::isfinite(value) && value>0,"Invalid local scale");
                for(const auto value:transform.orientation)Require(std::isfinite(value),"Nonfinite local orientation");
            }
            if(pendingActive || (operation==RuntimeOperation::Remove?!CanSubmitRemoval():!Ready()))return false;
            Require((operation==RuntimeOperation::Create && !logicalEntity) || (operation!=RuntimeOperation::Create && logicalEntity==entity),"Invalid local entity lifecycle");
            PublishCommand(operation,entity,transform,animation);return true;
        }
        bool RequestSceneFollow(const std::uint32_t levelId,const std::uint32_t entryId)
        {
            OwnerThread();Require(config.menuFirst && config.sceneFollow && config.startupLevel && levelId>=1 && levelId<=50 && entryId==0,
                "Native following requires an opted-in guest and a valid default-entry level");
            if(pendingActive || !Ready() || logicalEntity || snapshot.remoteEntity || snapshot.remoteCreated!=snapshot.remoteRemoved)return false;
            PublishCommand(RuntimeOperation::FollowLevel,0,{},RuntimeAnimation::Idle,levelId,entryId);return true;
        }
        void ParseOuter()
        {
            const auto document=Report(outerReport);Require(document.At("kind").String()=="XP32-original-game-graphics-probe","Wrong outer report kind");
            Require(document.At("pid").Unsigned(MAXDWORD)==identity.outerPid && SamePath(document.At("game").String(),Narrow(caseHost)) &&
                SamePath(document.At("media").String(),Narrow(config.media)),"Outer report does not match this owned case");
            const bool completed=document.At("probeCompleted").Boolean();const auto outerFailure=document.At("failure").String();
            const auto& registry=document.At("registry");const bool restoreAttempted=registry.At("restoreAttempted").Boolean(),verified=registry.At("restoreVerified").Boolean();
            const auto restoreError=registry.At("restoreError").Unsigned(MAXDWORD),verifyError=registry.At("verifyError").Unsigned(MAXDWORD);
            const auto& child=document.At("child");const bool created=child.At("created").Boolean(),stopped=child.At("stopped").Boolean();
            const bool timedOut=child.At("timedOut").Boolean(),terminated=child.At("terminateSucceeded").Boolean();
            const auto helperPid=child.At("pid").Unsigned(MAXDWORD),exitCode=child.At("exitCode").Unsigned(MAXDWORD),queryError=child.At("exitQueryError").Unsigned(MAXDWORD);
            Require(!identity.helperPid || helperPid==identity.helperPid,"Outer child PID differs from the bound helper");
            result.outerReportValid=true;result.outerProbeCompleted=completed;result.mediaRestoreAttempted=restoreAttempted;
            result.mediaRestoreVerified=restoreAttempted && verified && !restoreError && !verifyError;result.outerChildStopped=stopped;
            result.outerChildTimedOut=timedOut;result.outerTerminatedChild=terminated;result.helperExitCodeKnown=queryError==0;
            result.helperExitCode=static_cast<DWORD>(exitCode);
            Require(completed && outerFailure=="none" && created && stopped && !timedOut && !terminated && !queryError && result.mediaRestoreVerified,
                "Outer preparation/child cleanup/MediaPath rollback was not confirmed");
            Require(identity.helperPid && helperPid==identity.helperPid,"No matching helper status was observed for this outer child");
        }
        bool RetiredPlayingExit(const Json& document)
        {
            if(!config.menuFirst || !failure.empty() || document.At("readySamples").Unsigned(MAXDWORD)==0)return false;
            const auto ready=document.At("readySamples").Unsigned(MAXDWORD),samples=document.At("samples").Unsigned(MAXDWORD);
            Require(samples>=ready && document.At("runtimeReadyObserved").Boolean() &&
                document.At("loadComplete").Boolean() && document.At("startComplete").Boolean() &&
                document.At("fullContextRestored").Boolean() && document.At("privatePageFreed").Boolean() &&
                document.At("worldContextWitnessStarted").Boolean() && document.At("worldContextWitnessPassed").Boolean() &&
                document.At("worldContextWitnessGprPreserved").Boolean() && document.At("worldContextWitnessSegmentsPreserved").Boolean() &&
                document.At("worldContextWitnessDebugRegistersPreserved").Boolean() && document.At("worldContextWitnessFpuBytesPreserved").Boolean() &&
                document.At("worldContextWitnessSseBytesPreserved").Boolean() && document.At("worldContextWitnessAllExtendedBytesPreserved").Boolean() &&
                document.At("worldContextWitnessOriginalContextRestored").Boolean() && document.At("callerSignatureCapturedBeforeAdapter").Boolean() &&
                document.At("callerBreakpointRestored").Boolean() && !document.At("callerProtectionChangePending").Boolean(),
                "Playing exit lacks its actual bootstrap/world context qualification");
            Require(haveSnapshot && result.visualEpochRetired && snapshot.phase==RuntimePhase::Stopped && snapshot.epochRetired &&
                snapshot.error==RuntimeError::None && snapshot.levelId==0 && snapshot.localComponentCount==0 &&
                snapshot.worldUpdates>0 && !snapshot.remoteEntity && !snapshot.actorError && snapshot.remoteCreated==snapshot.remoteRemoved &&
                snapshot.unloadGuardHits>0 && snapshot.unloadCleanups<=snapshot.remoteRemoved &&
                document.At("runtimePhase").Unsigned(static_cast<unsigned>(RuntimePhase::Failed))==static_cast<unsigned>(snapshot.phase) &&
                document.At("runtimeError").Unsigned(static_cast<unsigned>(RuntimeError::Owner))==static_cast<unsigned>(snapshot.error) &&
                document.At("epochRetired").Boolean() && document.At("runtimeLevelId").Unsigned(MAXDWORD)==snapshot.levelId &&
                document.At("runtimeLocalComponentCount").Unsigned(MAXDWORD)==snapshot.localComponentCount &&
                document.At("worldUpdates").Unsigned(MAXDWORD)==snapshot.worldUpdates &&
                document.At("remoteCreated").Unsigned(MAXDWORD)==snapshot.remoteCreated && document.At("remoteRemoved").Unsigned(MAXDWORD)==snapshot.remoteRemoved &&
                document.At("remoteEntity").Unsigned(MAXDWORD)==snapshot.remoteEntity && document.At("actorError").Unsigned(MAXDWORD)==snapshot.actorError &&
                document.At("nativeCalls").Unsigned(MAXDWORD)==snapshot.nativeCalls &&
                document.At("unloadGuardHits").Unsigned(MAXDWORD)==snapshot.unloadGuardHits && document.At("unloadCleanups").Unsigned(MAXDWORD)==snapshot.unloadCleanups &&
                document.At("runtimeSceneGeneration").Unsigned(MAXDWORD)==snapshot.sceneGeneration &&
                document.At("runtimeSceneRetirements").Unsigned(MAXDWORD)==snapshot.sceneRetirements &&
                (!snapshot.remoteCreated || (snapshot.nativeCalls>0 && document.At("mediaQualifiedObserved").Boolean())),
                "Playing exit has no matching retired native scene/cleanup observation");
            RuntimeCommand command{};RuntimeAck ack{};
            Require(TryRead(runtime->command,command) && Same(&command,&lastPublished,sizeof(command)) &&
                TryRead(runtime->acknowledgement,ack) && Same(&ack,&acknowledgement,sizeof(ack)) &&
                command.sequence==result.commandsPublished && ack.sequence==result.commandsCompleted &&
                snapshot.lastCommandSequence==ack.sequence && command.sequence>=ack.sequence && command.sequence-ack.sequence<=1 &&
                document.At("commandsSent").Unsigned(MAXDWORD)==0 && document.At("commandsAcknowledged").Unsigned(MAXDWORD)==0 &&
                document.At("runtimeCommandsObserved").Boolean() && document.At("runtimeCommandStateConsistent").Boolean() &&
                document.At("runtimePublishedCommandSequence").Unsigned(MAXDWORD)==command.sequence &&
                document.At("runtimeAcknowledgedCommandSequence").Unsigned(MAXDWORD)==ack.sequence &&
                document.At("runtimeAcknowledgementStatus").Unsigned(static_cast<unsigned>(RuntimeAckStatus::CancelledOnRetirement))==static_cast<unsigned>(ack.status) &&
                document.At("runtimeAcknowledgementSceneGeneration").Unsigned(MAXDWORD)==ack.sceneGeneration &&
                document.At("runtimePublishedSceneGeneration").Unsigned(MAXDWORD)==command.sceneGeneration &&
                document.At("runtimeAcknowledgementWorldUpdates").Unsigned(MAXDWORD)==ack.worldUpdates &&
                document.At("runtimeLastCommandSequence").Unsigned(MAXDWORD)==ack.sequence,
                "Playing exit command/ACK evidence differs from the owned immutable mailboxes");
            const bool cancelled=command.sequence>ack.sequence;
            Require(cancelled?(pendingActive && Same(&pending,&command,sizeof(command))):!pendingActive,
                "Playing exit did not resolve its actual outstanding command state");
            Require(document.At("runtimeCommandCancelledOnRetirement").Boolean()==cancelled,
                "Playing exit cancellation differs from the real applied ACK sequence");
            // An outstanding command is still unconfirmed. Retirement cancels
            // its execution; neither the ACK nor the consumed sequence is forged.
            result.commandCancelledOnRetirement=cancelled;
            return true;
        }
        void ParseHelper()
        {
            const auto document=Report(hostReport);Require(document.At("kind").String()=="WinxCoop-owned-x86-game-host","Wrong helper report kind");
            Require(document.At("ownerPid").Unsigned(MAXDWORD)==identity.ownerPid && document.At("ownerCreationTime").Unsigned()==identity.ownerCreationTime &&
                document.At("ipcVersion").Unsigned(MAXDWORD)==IpcVersion && document.At("runtimeCommandWrites").Unsigned(MAXDWORD)==0 &&
                !document.At("controlledFpSeedPerformed").Boolean(),"Helper report binding/policy does not match this client");
            const bool mappingBound=document.At("externalRuntimeMappingBound").Boolean();
            Require(document.At("menuFirst").Boolean()==config.menuFirst &&
                document.At("gameLifetimeLimitEnabled").Boolean()!=config.menuFirst,"Helper lifetime policy differs from this launch");
            Require(document.At("startupLevel").Unsigned(50)==config.startupLevel &&
                document.At("startupEntry").Unsigned(0)==config.startupEntry,"Helper startup selection differs from its immutable assigned scene");
            Require(document.At("sceneFollow").Boolean()==config.sceneFollow,"Helper scene-follow policy differs from this launch");
            if(config.menuFirst)
                Require(document.At("helperPid").Unsigned(MAXDWORD)==identity.helperPid &&
                    document.At("helperCreationTime").Unsigned()==identity.helperCreationTime,"Direct helper report names another process instance");
            const auto gamePid=document.At("pid").Unsigned(MAXDWORD);
            Require(!identity.gamePid || gamePid==identity.gamePid,"Helper report game PID differs from the bound runtime");
            if(config.menuFirst)
            {
                identity.gamePid=static_cast<DWORD>(gamePid);
                identity.gameCreationTime=document.At("gameCreationTime").Unsigned();
                const bool created=document.At("gameProcessCreated").Boolean();
                Require(created==(gamePid!=0),"Helper process-created witness differs from its original PID");
                result.originalGameStarted=created;
            }
            const bool completed=document.At("serviceCompleted").Boolean(),known=document.At("exitCodeKnown").Boolean();
            const bool continued=document.At("exitEventContinued").Boolean(),cleaned=document.At("cleanupConfirmed").Boolean(),forced=document.At("forcedTermination").Boolean();
            const auto exitCode=document.At("exitCode").Unsigned(MAXDWORD),reason=document.At("stopReason").Unsigned(static_cast<unsigned>(GameHostStopReason::Error));
            const bool limited=document.At("limitReached").Boolean();const auto& error=document.At("error");
            Require(error.kind==Json::Kind::Null || error.kind==Json::Kind::String,"Invalid helper error field");
            result.helperReportValid=true;result.helperCompleted=completed;result.gameExitCodeKnown=known;result.gameExitCode=static_cast<DWORD>(exitCode);
            result.gameExitEventContinued=continued;result.gameCleanupConfirmed=cleaned && !config.menuFirst;result.gameForcedTermination=forced;result.helperLimitReached=limited;
            result.stopReason=static_cast<GameHostStopReason>(reason);
            if(config.menuFirst && cleaned && known && continued && identity.gamePid && identity.gameCreationTime)
            {
                Require(identity.gamePid!=identity.helperPid && identity.gamePid!=identity.ownerPid,"GameHost report does not name its original child");
                Handle game;game.value=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,identity.gamePid);
                if(!game.Valid())
                    Require(GetLastError()==ERROR_INVALID_PARAMETER,"Original game exit cannot be independently queried");
                else
                {
                    const auto created=ProcessCreation(game.value);
                    // A reused PID proves the reported original instance is gone;
                    // never stop, modify or mistake the unrelated new process.
                    if(created==identity.gameCreationTime)
                        Require(WaitForSingleObject(game.value,0)==WAIT_OBJECT_0,"Reported original game instance is still running");
                }
                result.gameExitIndependentlyVerified=true;result.gameCleanupConfirmed=true;
            }
            else if(config.menuFirst && cleaned && !identity.gamePid && !identity.gameCreationTime &&
                !document.At("loadComplete").Boolean() && !document.At("startComplete").Boolean())
                result.gameCleanupConfirmed=true; // Bound helper report confirms no original process was created.
            if(error.kind==Json::Kind::String)throw std::runtime_error("GameHost: "+error.String());
            Require(mappingBound,"GameHost never bound the expected native mappings");
            // Mirror the measured original menu exit; the helper process must
            // still exit0. These are raw observations, never a synthetic exit0.
            const bool measuredMenuExit=config.menuFirst && known && continued && exitCode==1 && !forced &&
                !document.At("stopRequested").Boolean() && reason==static_cast<unsigned>(GameHostStopReason::GameExit) &&
                document.At("fatalExceptions").Unsigned(MAXDWORD)==0 && document.At("samples").Unsigned(MAXDWORD)>0 &&
                document.At("readySamples").Unsigned(MAXDWORD)==0 && document.At("runtimeLevelId").Unsigned(MAXDWORD)==0 &&
                document.At("runtimeLastCommandSequence").Unsigned(MAXDWORD)==0 && document.At("commandsSent").Unsigned(MAXDWORD)==0 &&
                document.At("commandsAcknowledged").Unsigned(MAXDWORD)==0 && document.At("remoteCreated").Unsigned(MAXDWORD)==0 &&
                document.At("remoteRemoved").Unsigned(MAXDWORD)==0 && document.At("remoteEntity").Unsigned(MAXDWORD)==0 &&
                result.commandsPublished==0 && !pendingActive;
            const bool measuredPlayingExit=config.menuFirst && completed && known && continued && cleaned && exitCode==1 && !forced &&
                !document.At("stopRequested").Boolean() && reason==static_cast<unsigned>(GameHostStopReason::GameExit) &&
                document.At("fatalExceptions").Unsigned(MAXDWORD)==0 && RetiredPlayingExit(document);
            Require(completed && known && continued && cleaned && (exitCode==0 || measuredMenuExit || measuredPlayingExit) && error.kind==Json::Kind::Null,"GameHost/native owned cleanup failed");
            if(config.menuFirst)
                Require(!limited && identity.gamePid && identity.gameCreationTime && result.gameExitIndependentlyVerified,
                    "Direct GameHost lacks an unlimited independently retired original child identity");
        }
        void Finalize()
        {
            if(finalized)return;
            if(config.menuFirst)
            {
                result.helperExited=true;DWORD code{};
                try
                {
                    VerifyDirectHost();
                    if(!GetExitCodeProcess(outerProcess.value,&code))WinError("Direct GameHost exit code unavailable");
                    result.helperExitCodeKnown=true;result.helperExitCode=code;
                    // The helper may finish between GUI ticks. Consume the
                    // final native snapshot and real ACK before its report;
                    // a retired epoch is distinct from a synthetic ACK.
                    try { Headers();ReadSnapshot();ReadAck(); }
                    catch(const std::exception& error){Fail(error.what());}
                    // Preserve the first native failure, but still parse the
                    // independently bound report to establish process cleanup.
                    ParseHelper();
                }
                catch(const std::exception& error){Fail(error.what());}
                result.commandUnconfirmed=pendingActive;result.completed=true;
                result.directCleanupVerified=result.helperExited && result.helperExitCodeKnown && result.helperReportValid && result.gameCleanupConfirmed &&
                    (result.gameExitIndependentlyVerified || !result.originalGameStarted);
                if(result.helperExitCodeKnown && result.helperExitCode!=0)Fail("Direct GameHost exited with a failure code");
                result.successful=failure.empty() && result.helperExitCodeKnown && result.helperExitCode==0 &&
                    result.helperReportValid && result.helperCompleted && result.gameCleanupConfirmed && result.gameExitIndependentlyVerified;
                finalized=true;phase=result.successful?GameProcessClientPhase::Closed:GameProcessClientPhase::Failed;result.error=failure;return;
            }
            result.outerExited=true;DWORD code{};if(!GetExitCodeProcess(outerProcess.value,&code))Fail("Outer exit code unavailable");
            else{result.outerExitCodeKnown=true;result.outerExitCode=code;}
            try{ParseOuter();}catch(const std::exception& error){Fail(error.what());}
            try{ParseHelper();}catch(const std::exception& error){Fail(error.what());}
            result.commandUnconfirmed=pendingActive;result.completed=true;
            if(result.outerExitCodeKnown && result.outerExitCode!=0)Fail("Outer helper exited with a failure code");
            if(result.helperExitCodeKnown && result.helperExitCode!=0)Fail("GameHost exited with a failure code");
            if(!ReleaseMediaLease())Fail("Parent MediaPath lease handle did not close");
            result.successful=failure.empty() && result.outerReportValid && result.helperReportValid && result.mediaRestoreVerified && result.outerChildStopped &&
                result.gameCleanupConfirmed && result.mediaLeaseInherited && result.mediaLeaseReleased;
            finalized=true;phase=result.successful?GameProcessClientPhase::Closed:GameProcessClientPhase::Failed;result.error=failure;
        }
        void PollOuter()
        {
            const DWORD wait=WaitForSingleObject(outerProcess.value,0);
            if(wait==WAIT_OBJECT_0)Finalize();else Require(wait==WAIT_TIMEOUT,config.menuFirst?"Owned direct GameHost wait failed":"Owned outer wait failed");
        }
        void Pump()
        {
            OwnerThread();if(!OwnerProcessStarted() || finalized)return;PollOuter();if(finalized)return;
            VerifyDirectHost();
            Headers();ReadHelper();ReadSnapshot();ReadAck();
            RuntimeCommand actual{};
            if(TryRead(runtime->command,actual))Require(Same(&actual,&lastPublished,sizeof(actual)),"Published command was changed or cleared");
            const auto now=GetTickCount64();
            if(pendingActive && !stopRequested && !(config.menuFirst && result.visualEpochRetired))
            {
                // Stock cinematics/loading may stop World entirely. A stale
                // observation does not establish native Ready time and must
                // never turn an ordinary cutscene into a stop/forced exit.
                // Use the same native readiness admission as normal commands;
                // the guest's additional follow barrier is not a native stall.
                const bool eligible=!config.menuFirst || NativeReadyForCommandProgress();
                if(now>=commandLastCheck && eligible)commandReadyWait+=config.menuFirst?
                    std::min<ULONGLONG>(now-commandLastCheck,1000):now-commandLastCheck;
                commandLastCheck=now;
                Require(commandReadyWait<=CommandDeadline,"Native command acknowledgement timed out during ready time");
            }
            if(!config.menuFirst)Require(now-began<=std::uint64_t(config.seconds+45)*1000,"Outer deadline exceeded; rollback remains unverified until it exits");
            if(stopRequested)phase=GameProcessClientPhase::Stopping;
            else if(Ready())phase=GameProcessClientPhase::Ready;
            else phase=haveHelper && helper.phase==GameHostPhase::Running?GameProcessClientPhase::WaitingForLevel:GameProcessClientPhase::Starting;
            PollOuter();
        }
        void PumpFailure(const std::string& message)
        {
            Fail(message);
            if(OwnerProcessStarted()){try{PollOuter();}catch(...){} }
            else
            {
                ReleaseMediaLease();result.completed=true;phase=GameProcessClientPhase::Failed;finalized=true;
                result.directCleanupVerified=config.menuFirst; // No owner/helper was created, so no original game can exist.
            }
        }
    };

    GameProcessClient::GameProcessClient(GameProcessClientConfiguration configuration):implementation_(std::make_unique<Impl>(std::move(configuration))){}
    GameProcessClient::~GameProcessClient()=default;
    bool GameProcessClient::Start() noexcept
    {
        auto& client=*implementation_;if(client.attempted)return false;
        try{client.Start();return true;}catch(const std::exception& error){client.PumpFailure(error.what());}catch(...){client.PumpFailure("Unknown client start failure");}return false;
    }
    void GameProcessClient::Pump() noexcept
    {
        auto& client=*implementation_;try{client.Pump();}catch(const std::exception& error){client.PumpFailure(error.what());}catch(...){client.PumpFailure("Unknown client pump failure");}
    }
    bool GameProcessClient::Submit(const RuntimeOperation operation,const std::uint32_t entity,const Transform& transform,const RuntimeAnimation animation) noexcept
    {
        auto& client=*implementation_;try{return client.Submit(operation,entity,transform,animation);}catch(const std::exception& error){client.PumpFailure(error.what());}catch(...){client.PumpFailure("Unknown client command failure");}return false;
    }
    bool GameProcessClient::RequestSceneFollow(const std::uint32_t levelId,const std::uint32_t entryId) noexcept
    {
        auto& client=*implementation_;try{return client.RequestSceneFollow(levelId,entryId);}catch(const std::exception& error){client.PumpFailure(error.what());}
        catch(...){client.PumpFailure("Unknown client scene-follow failure");}return false;
    }
    void GameProcessClient::RequestStop() noexcept {implementation_->Stop();}
    GameProcessClientPhase GameProcessClient::Phase() const noexcept {return implementation_->phase;}
    bool GameProcessClient::Ready() const noexcept {return implementation_->Ready();}
    bool GameProcessClient::CanSubmitRemoval() const noexcept {return implementation_->CanSubmitRemoval();}
    bool GameProcessClient::HasRuntimeSnapshot() const noexcept {return implementation_->haveSnapshot;}
    bool GameProcessClient::CommandPending() const noexcept {return implementation_->pendingActive;}
    bool GameProcessClient::Finished() const noexcept {return implementation_->finalized;}
    const RuntimeSnapshot& GameProcessClient::Snapshot() const noexcept {return implementation_->snapshot;}
    const RuntimeAck& GameProcessClient::Acknowledgement() const noexcept {return implementation_->acknowledgement;}
    const GameHostStatusSnapshot& GameProcessClient::HelperStatus() const noexcept {return implementation_->helper;}
    const GameProcessClientIdentity& GameProcessClient::Identity() const noexcept {return implementation_->identity;}
    const GameProcessClientResult& GameProcessClient::Result() const noexcept {return implementation_->result;}
    const std::string& GameProcessClient::Failure() const noexcept {return implementation_->failure;}
}
