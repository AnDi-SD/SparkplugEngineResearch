// Own external-client diagnostic. The copied helper performs the sole game
// bootstrap; the existing outer performs the sole MediaPath transaction.
#include "../Launcher/GameProcessClient.h"
#include <shellapi.h>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace winxcoop::adapter;
    using namespace winxcoop::launcher;
    struct Handle final {HANDLE value{};~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);} };
    void Require(const bool value,const char* text){if(!value)throw std::runtime_error(text);}
    std::uint64_t Number(const std::wstring& value,const std::uint64_t maximum)
    {
        Require(!value.empty(),"Missing decimal option");std::uint64_t result{};
        for(const auto c:value){Require(c>=L'0' && c<=L'9',"Decimal option contains unsupported characters");const auto digit=static_cast<unsigned>(c-L'0');
            Require(digit<=maximum && result<=(maximum-digit)/10,"Decimal option overflow");result=result*10+digit;}return result;
    }
    struct Options final
    {
        GameProcessClientConfiguration configuration;
        std::wstring mode=L"lifecycle";
        std::uint32_t holdMilliseconds=1000;
    };
    Options Parse()
    {
        int count{};wchar_t** raw=CommandLineToArgvW(GetCommandLineW(),&count);Require(raw!=nullptr,"Command line unavailable");
        struct Args {wchar_t** value;~Args(){if(value)LocalFree(value);}} args{raw};std::wstring sidecar;
        if(count==1)
        {
            wchar_t path[MAX_PATH]{};const auto length=GetModuleFileNameW(nullptr,path,MAX_PATH);Require(length && length<MAX_PATH-5,"Probe path exceeds bound");
            Handle file;file.value=CreateFileW((std::wstring(path)+L".args").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            Require(file.value && file.value!=INVALID_HANDLE_VALUE,"Adjacent .exe.args unavailable");LARGE_INTEGER size{};
            Require(GetFileSizeEx(file.value,&size) && size.QuadPart>0 && size.QuadPart<=4096,"Probe sidecar exceeds bound");
            std::vector<char> bytes(static_cast<std::size_t>(size.QuadPart));DWORD got{};
            Require(ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) && got==bytes.size(),"Probe sidecar read failed");
            sidecar=L"GameHostClientProbe.exe ";for(const auto value:bytes)
            {const auto c=static_cast<unsigned char>(value);Require((c>=32 && c<=126) || c=='\r' || c=='\n' || c=='\t',"Probe sidecar must be ASCII");sidecar+=wchar_t(c);}
            LocalFree(args.value);args.value=nullptr;raw=CommandLineToArgvW(sidecar.c_str(),&count);Require(raw!=nullptr,"Probe sidecar command line invalid");args.value=raw;
        }
        Options options;std::vector<std::wstring> seen;
        for(int i=1;i<count;++i)
        {
            const std::wstring key=raw[i];Require(i+1<count,"Each probe option requires a value");const std::wstring value=raw[++i];
            for(const auto& prior:seen)Require(prior!=key,"Repeated probe option");seen.push_back(key);
            if(key==L"--game")options.configuration.game=value;else if(key==L"--media")options.configuration.media=value;
            else if(key==L"--adapter")options.configuration.adapter=value;else if(key==L"--host")options.configuration.gameHost=value;
            else if(key==L"--outer")options.configuration.outer=value;else if(key==L"--case")options.configuration.caseDirectory=value;
            else if(key==L"--session")options.configuration.session=Number(value,std::numeric_limits<std::uint64_t>::max());
            else if(key==L"--epoch")options.configuration.levelEpoch=static_cast<DWORD>(Number(value,MAXDWORD));
            else if(key==L"--seconds")options.configuration.seconds=static_cast<DWORD>(Number(value,3600));
            else if(key==L"--mode")options.mode=value;else if(key==L"--hold-ms")options.holdMilliseconds=static_cast<DWORD>(Number(value,60000));
            else throw std::runtime_error("Unknown probe option");
        }
        Require(options.mode==L"lifecycle" || options.mode==L"snapshot-stop" || options.mode==L"owner-exit","Unknown external-client probe mode");return options;
    }
    std::string Boolean(const bool value){return value?"true":"false";}
    std::string Quote(const std::string& value)
    {
        std::string result="\"";for(const auto c:value){if(c=='"' || c=='\\')result+='\\';result+=static_cast<unsigned char>(c)>=32?c:'?';}return result+'"';
    }
    void WriteReport(const std::wstring& path,const std::string& value)
    {
        Handle file;file.value=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file.value && file.value!=INVALID_HANDLE_VALUE,"Fresh client-probe report unavailable");DWORD written{};
        Require(WriteFile(file.value,value.data(),static_cast<DWORD>(value.size()),&written,nullptr) && written==value.size() && FlushFileBuffers(file.value),"Client-probe report write failed");
    }
    std::string IdentityFields(const GameProcessClientIdentity& identity)
    {
        return "\"ownerPid\":"+std::to_string(identity.ownerPid)+",\"ownerCreationTime\":"+std::to_string(identity.ownerCreationTime)+
            ",\"nonce\":"+std::to_string(identity.nonce)+",\"session\":"+std::to_string(identity.session)+",\"epoch\":"+std::to_string(identity.levelEpoch)+
            ",\"outerPid\":"+std::to_string(identity.outerPid)+",\"helperPid\":"+std::to_string(identity.helperPid)+",\"gamePid\":"+std::to_string(identity.gamePid)+
            ",\"mainThreadId\":"+std::to_string(identity.mainThreadId);
    }
    int Run()
    {
        const Options options=Parse();GameProcessClient client(options.configuration);const bool started=client.Start();
        unsigned stage{};bool readyObserved{},scriptComplete{};ULONGLONG stageBegan=GetTickCount64(),began=stageBegan;Transform remote{};
        while(started && !client.Finished())
        {
            client.Pump();const auto now=GetTickCount64();
            if(!client.Failure().empty())client.RequestStop();
            else if(stage==0 && client.Ready())
            {
                readyObserved=true;remote=client.Snapshot().local;remote.position[0]+=180;
                if(options.mode==L"snapshot-stop"){stage=10;stageBegan=now;}
                else if(client.Submit(RuntimeOperation::Create,2,remote,RuntimeAnimation::Idle)){stage=1;stageBegan=now;}
            }
            else if(stage==1 && client.Acknowledgement().sequence==1)
            {
                if(options.mode==L"owner-exit")
                {
                    if(now-stageBegan>=options.holdMilliseconds)
                    {
                        const auto text="{\"kind\":\"WinxCoop-game-host-client-owner-exit-intent\",\"diagnosticOnly\":true,\"gameplayAccepted\":false,"+
                            IdentityFields(client.Identity())+",\"createAcknowledged\":true,\"rollbackVerifiedByOwner\":false,\"outerMustOutliveOwner\":true,\"mediaLeaseInherited\":"+
                            Boolean(client.Result().mediaLeaseInherited)+"}\n";
                        WriteReport(options.configuration.caseDirectory+L"\\owner-exit-intent.json",text);
                        // Intentional process exit skips the facade destructor:
                        // service/DLL owner handles and surviving outer are the
                        // components under test. Root verifies their reports.
                        ExitProcess(0);
                    }
                }
                else if(now-stageBegan>=options.holdMilliseconds && client.Ready())
                {remote.position[0]+=90;if(client.Submit(RuntimeOperation::Apply,2,remote,RuntimeAnimation::Walk)){stage=2;stageBegan=now;}}
            }
            else if(stage==2 && client.Acknowledgement().sequence==2 && now-stageBegan>=250 && client.Ready())
            {if(client.Submit(RuntimeOperation::Remove,2)){stage=3;stageBegan=now;}}
            else if(stage==3 && client.Acknowledgement().sequence==3)
            {scriptComplete=true;stage=4;client.RequestStop();}
            else if(stage==10 && now-stageBegan>=options.holdMilliseconds)
            {scriptComplete=true;stage=11;client.RequestStop();}
            if(now-began>std::uint64_t(options.configuration.seconds+55)*1000){client.RequestStop();break;}
            if(!client.Finished())Sleep(10);
        }
        const auto& result=client.Result();const bool complete=started && readyObserved && scriptComplete && client.Finished() && result.successful;
        const auto text="{\"kind\":\"WinxCoop-game-host-external-client-probe\",\"diagnosticOnly\":true,\"gameplayAccepted\":false,\"probeCompleted\":"+Boolean(complete)+
            ",\"mode\":"+Quote(options.mode==L"lifecycle"?"lifecycle":options.mode==L"snapshot-stop"?"snapshot-stop":"owner-exit")+','+IdentityFields(client.Identity())+
            ",\"readyObserved\":"+Boolean(readyObserved)+",\"scriptComplete\":"+Boolean(scriptComplete)+",\"clientFinished\":"+Boolean(client.Finished())+
            ",\"clientSuccessful\":"+Boolean(result.successful)+",\"outerStarted\":"+Boolean(result.outerStarted)+",\"outerExited\":"+Boolean(result.outerExited)+
            ",\"outerExitCodeKnown\":"+Boolean(result.outerExitCodeKnown)+",\"outerExitCode\":"+std::to_string(result.outerExitCode)+
            ",\"outerReportValid\":"+Boolean(result.outerReportValid)+",\"mediaRestoreVerified\":"+Boolean(result.mediaRestoreVerified)+
            ",\"mediaLeaseAcquired\":"+Boolean(result.mediaLeaseAcquired)+",\"mediaLeaseInherited\":"+Boolean(result.mediaLeaseInherited)+",\"mediaLeaseReleased\":"+Boolean(result.mediaLeaseReleased)+
            ",\"helperReportValid\":"+Boolean(result.helperReportValid)+",\"helperCompleted\":"+Boolean(result.helperCompleted)+
            ",\"helperExitCodeKnown\":"+Boolean(result.helperExitCodeKnown)+",\"helperExitCode\":"+std::to_string(result.helperExitCode)+
            ",\"gameExitCodeKnown\":"+Boolean(result.gameExitCodeKnown)+",\"gameExitCode\":"+std::to_string(result.gameExitCode)+
            ",\"gameExitEventContinued\":"+Boolean(result.gameExitEventContinued)+",\"gameCleanupConfirmed\":"+Boolean(result.gameCleanupConfirmed)+
            ",\"gameForcedTermination\":"+Boolean(result.gameForcedTermination)+",\"commandsPublished\":"+std::to_string(result.commandsPublished)+
            ",\"commandsAcknowledged\":"+std::to_string(result.commandsAcknowledged)+",\"commandUnconfirmed\":"+Boolean(result.commandUnconfirmed)+
            ",\"error\":"+(client.Failure().empty()?std::string("null"):Quote(client.Failure()))+"}\n";
        if(GetFileAttributesW(options.configuration.caseDirectory.c_str())!=INVALID_FILE_ATTRIBUTES)WriteReport(options.configuration.caseDirectory+L"\\client-report.json",text);
        std::fputs(text.c_str(),stdout);return complete?0:4;
    }
}
int wmain()
{
    try{return Run();}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 3;}catch(...){return 3;}
}
