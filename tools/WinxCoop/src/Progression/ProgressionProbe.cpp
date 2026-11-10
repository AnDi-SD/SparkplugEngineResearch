// Two real TCP processes, explicit native-receipt fixtures, no Winx process.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "ProgressionSession.h"
#include "Networking/WindowsTcpTransport.h"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace p=winxcoop::progression;
namespace n=winxcoop::networking;
namespace
{
    constexpr std::uint32_t CompletionClass=0x36545057;
    constexpr unsigned char Completion[]={'E','6','O','K'};
    void Require(bool value,const char* text) { if(!value)throw std::runtime_error(text); }
    std::uint64_t Number(const std::wstring& value,std::uint64_t maximum)
    {
        std::string text;
        for(wchar_t digit:value)
        {Require(digit>=L'0'&&digit<=L'9',"Only decimal ASCII digits are accepted");text.push_back(static_cast<char>(digit));}
        std::uint64_t result{};
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
        Require(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&result<=maximum,"Invalid bounded integer");return result;
    }
    std::string Quote(const std::string& value)
    {
        std::string result="\"";
        for(unsigned char c:value){Require(c>=32,"Control character in report");if(c=='"'||c=='\\')result+='\\';result+=static_cast<char>(c);}
        return result+'"';
    }
    void FreshFile(const std::filesystem::path& path,const std::string& text)
    {
        const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"Fresh output required");DWORD written{};
        const bool ok=text.size()<=MAXDWORD&&WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();
        const bool closed=CloseHandle(file)!=0;Require(ok&&closed,"Output write/close failed");
    }
    p::Digest Digest(unsigned value)
    {p::Digest r{};for(unsigned i=0;i<r.size();++i)r[i]=static_cast<unsigned char>(value+i*7);return r;}
    p::ChannelGeneration Channels(std::uint64_t cycle)
    {const auto v=1000+cycle*100;return {v+1,v+2,v+3,v+4,v+5,v+6,v+7};}
    p::LevelDescriptor Descriptor(unsigned level){return {level,0,Digest(level)};}
    p::NativeRetirementReceipt Retirement(p::SceneIdentity scene)
    {return {scene,true,true,true,true,true,true,true,true,true};}
    p::NativeEntryReceipt Entry(const p::Decision& d,const p::ChannelGeneration& channels,
        const p::Digest& seed,const p::Digest& witness,bool host)
    {return {d.next,d.destination,channels,seed,witness,host?2ull:1ull,true,true};}
    struct Options
    {
        p::Role role=p::Role::Host;bool roleSet=false,withholdGuest=false;
        std::uint16_t port{};std::filesystem::path report,ready;
        std::size_t sendChunk=7,receiveChunk=5;
    };
    Options Parse(int argc,wchar_t** argv)
    {
        Options o;
        for(int i=1;i<argc;++i)
        {
            const std::wstring key=argv[i];Require(i+1<argc,"Missing option value");const std::wstring value=argv[++i];
            if(key==L"--role"){Require(value==L"host"||value==L"guest","Invalid role");o.role=value==L"host"?p::Role::Host:p::Role::Guest;o.roleSet=true;}
            else if(key==L"--server-port")o.port=static_cast<std::uint16_t>(Number(value,65535));
            else if(key==L"--report")o.report=value;
            else if(key==L"--ready-file")o.ready=value;
            else if(key==L"--send-chunk")o.sendChunk=static_cast<std::size_t>(Number(value,65536));
            else if(key==L"--receive-chunk")o.receiveChunk=static_cast<std::size_t>(Number(value,65536));
            else if(key==L"--scenario"){Require(value==L"complete"||value==L"withhold-guest","Invalid scenario");o.withholdGuest=value==L"withhold-guest";}
            else throw std::invalid_argument("Unknown option");
        }
        Require(o.roleSet&&!o.report.empty()&&o.report.is_absolute()&&o.sendChunk&&o.receiveChunk,"Explicit role/report and positive chunks required");
        Require(o.role==p::Role::Host?!o.ready.empty()&&o.ready.is_absolute()&&!o.port:o.port&&o.ready.empty(),"Host ready file or Guest server port required");
        Require(!std::filesystem::exists(o.report)&&(o.ready.empty()||!std::filesystem::exists(o.ready)),"Fresh report/ready paths required");return o;
    }
    class Exercise
    {
    public:
        explicit Exercise(Options options):options_(std::move(options)),tcp_(TcpConfig()),
            barrier_(options_.role,{0xE6000001,0xE6000002,0xE6000003},{{1,1},Descriptor(28),Channels(0)},2500){}
        int Run()
        {
            const auto start=GetTickCount64();
            try
            {
                Require(tcp_.Start(start),"TCP start failed");
                if(options_.role==p::Role::Host)FreshFile(options_.ready,"{\"port\":"+std::to_string(tcp_.BoundEndpoint().port)+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"nativeGameStarted\":false}");
                while(GetTickCount64()-start<15000)
                {
                    const auto now=GetTickCount64();tcp_.Pump(now);while(tcp_.TakeSessionEvent()){}
                    if(!connection_&&tcp_.ActiveConnectionId())connection_=tcp_.ActiveConnectionId();
                    const auto* core=tcp_.Session();
                    if(options_.role==p::Role::Host&&core&&core->Phase()==n::SessionPhase::WaitingForHost)
                        Require(tcp_.HostStart(0xE606,now),"Transport authorization failed");
                    while(const auto received=tcp_.TakeMessage())
                    {
                        Require(received->connectionId==connection_,"Foreign connection delivered");
                        if(received->message.classId==CompletionClass)
                        {
                            Require(received->message.key==1&&received->message.payload==std::vector<std::uint8_t>(std::begin(Completion),std::end(Completion)),"Invalid completion marker");
                            Require(barrier_.CompletedOperation()==2&&barrier_.InputAllowed(),"Peer completed before barrier");peerDone_=true;continue;
                        }
                        const auto message=p::Decode(received->message);Require(message.has_value(),"Progression decode failed");
                        Require(barrier_.Receive(*message,now),"Progression receive rejected");++received_;
                        if(message->kind==p::Kind::Prepare)BeginFixture(message->decision,now);
                    }
                    if(barrier_.Failure()!=p::Terminal::None)break;
                    core=tcp_.Session();
                    if(core&&core->Phase()==n::SessionPhase::Running)
                    {
                        if(options_.role==p::Role::Host&&barrier_.InputAllowed()&&barrier_.CompletedOperation()<2)
                        {
                            const auto op=barrier_.CompletedOperation()+1;
                            p::Decision d{op,barrier_.CurrentScene(),{static_cast<std::uint32_t>(op+1),static_cast<std::uint32_t>(op+1)},
                                op==1?p::Reason::LevelChange:p::Reason::Reload,Descriptor(47)};
                            Require(barrier_.Begin(d,now),"Host prepare failed");BeginFixture(d,now);
                        }
                        if(decision_&&!retired_&&now-decisionStarted_>=(options_.role==p::Role::Guest?120u:20u))
                        {Require(barrier_.ObserveLocalRetired(Retirement(decision_->previous),now),"Retirement fixture rejected");retired_=true;++retirementFixtures_;}
                        if(decision_&&retired_&&options_.role==p::Role::Host&&!hostEntry_)
                        {
                            Require(barrier_.ObserveHostEntry(Entry(*decision_,Channels(decision_->operation),Digest(static_cast<unsigned>(decision_->operation+20)),Digest(static_cast<unsigned>(decision_->operation+40)),true),now),"Host entry fixture rejected");
                            hostEntry_=true;++entryFixtures_;
                        }
                        if(options_.role==p::Role::Guest)
                        if(const auto load=barrier_.TakeLoadAuthorization(now))
                        {
                            Require(options_.role==p::Role::Guest&&retired_&&decision_&&load->decision==*decision_,"Load crossed local retirement barrier");++loadAuthorizations_;
                            if(!options_.withholdGuest)
                            {
                                p::NativeWorldAppliedReceipt applied{load->decision.next,load->channels,load->decision.operation*100,load->decision.operation*300,true,true};
                                Require(barrier_.ObserveGuestEntryApplied(Entry(load->decision,load->channels,load->seed,load->witness,false),applied,now),"Guest entry fixture rejected");++entryFixtures_;
                            }
                        }
                        while(const auto message=barrier_.PeekOutbound())
                        {
                            const auto bytes=p::Encode(*message);Require(bytes.has_value(),"Progression encode failed");
                            if(!tcp_.SendApplicationMessage(tcp_.ActiveConnectionId(),p::ApplicationClass,static_cast<std::uint32_t>(message->kind),bytes->data(),static_cast<std::uint16_t>(bytes->size()),now))break;
                            Require(barrier_.CommitOutbound(now),"Progression outbound commit failed");++sent_;
                        }
                        if(barrier_.CompletedOperation()==2&&barrier_.InputAllowed()&&!doneSent_)
                            doneSent_=tcp_.SendApplicationMessage(tcp_.ActiveConnectionId(),CompletionClass,1,Completion,sizeof(Completion),now);
                        if(doneSent_&&peerDone_&&!tcp_.PendingSendBytes()&&!core->OutboundCount()){passed_=true;break;}
                    }
                    if(!barrier_.Poll(now))break;
                    if(!core&&tcp_.LastTerminal())
                    {Require(doneSent_&&peerDone_,"Transport retired before completion");passed_=true;break;}
                    if(core&&(core->Phase()==n::SessionPhase::Closed||core->Phase()==n::SessionPhase::Failed))
                    {Require(doneSent_&&peerDone_,"Transport closed before completion");passed_=true;break;}
                    Sleep(1);
                }
                if(options_.withholdGuest)passed_=barrier_.Failure()==p::Terminal::Timeout&&!barrier_.InputAllowed()&&!barrier_.CompletedOperation();
                Require(passed_,"Scenario did not reach expected terminal condition");
            }
            catch(const std::exception& e){failure_=e.what();passed_=false;}
            // Keep the transport alive briefly after the Host's own deadline,
            // so the Guest can exercise its independently started deadline.
            // The progression owner stays terminal and admits no work here.
            if(options_.withholdGuest&&options_.role==p::Role::Host&&barrier_.Failure()==p::Terminal::Timeout)
            {
                const auto drain=GetTickCount64();
                while(GetTickCount64()-drain<1000)
                {Require(!barrier_.InputAllowed(),"Timed-out barrier reopened input");tcp_.Pump(GetTickCount64());while(tcp_.TakeMessage()){};Sleep(1);}
                timeoutDrain_=GetTickCount64()-drain;
            }
            tcp_.Close();const auto m=tcp_.Metrics();
            const bool cleanup=!tcp_.OwnedSocketCount()&&m.socketsOpened==m.socketsClosed&&m.wsaStarts==m.wsaCleanups&&!m.socketCloseErrors&&!m.wsaCleanupErrors;passed_=passed_&&cleanup;
            const auto report="{\"kind\":\"WinxCoopProgressionTcpProbe\",\"passed\":"+std::string(passed_?"true":"false")+
                ",\"nativeGameStarted\":false,\"nativeReceiptsAreFixtures\":true,\"gameplayQualified\":false,\"role\":"+Quote(options_.role==p::Role::Host?"host":"guest")+
                ",\"scenario\":"+Quote(options_.withholdGuest?"withhold-guest":"complete")+",\"failure\":"+(failure_.empty()?"null":Quote(failure_))+
                ",\"completedOperations\":"+std::to_string(barrier_.CompletedOperation())+",\"terminal\":"+std::to_string(static_cast<unsigned>(barrier_.Failure()))+
                ",\"inputAllowedAtEnd\":"+(barrier_.InputAllowed()?"true":"false")+",\"retirementFixtures\":"+std::to_string(retirementFixtures_)+
                ",\"entryFixtures\":"+std::to_string(entryFixtures_)+",\"loadAuthorizations\":"+std::to_string(loadAuthorizations_)+
                ",\"sent\":"+std::to_string(sent_)+",\"received\":"+std::to_string(received_)+",\"elapsedMilliseconds\":"+std::to_string(GetTickCount64()-start)+
                ",\"socketCleanupVerified\":"+(cleanup?"true":"false")+",\"socketsOpened\":"+std::to_string(m.socketsOpened)+",\"socketsClosed\":"+std::to_string(m.socketsClosed)+
                ",\"sendRequestCapped\":"+std::to_string(m.sendRequestCapped)+",\"framePartialAdvances\":"+std::to_string(m.framePartialAdvances)+
                ",\"timeoutTransportDrainMilliseconds\":"+std::to_string(timeoutDrain_)+"}";
            FreshFile(options_.report,report);std::cout<<report<<'\n';return passed_?0:1;
        }
    private:
        n::TcpTransportConfig TcpConfig()const
        {
            n::TcpTransportConfig c;c.role=options_.role==p::Role::Host?n::SessionRole::Host:n::SessionRole::Guest;c.localBind={0x7F000001,0};
            if(options_.role==p::Role::Guest)c.server=n::Endpoint{0x7F000001,options_.port};
            c.maxSendChunk=options_.sendChunk;c.maxReceiveChunk=options_.receiveChunk;c.heartbeatMilliseconds=500;c.timeoutMilliseconds=5000;return c;
        }
        void BeginFixture(const p::Decision& d,std::uint64_t now)
        {Require(!barrier_.InputAllowed(),"Input remained admitted at Prepare");decision_=d;decisionStarted_=now;retired_=false;hostEntry_=false;}
        Options options_;n::WindowsTcpTransport tcp_;p::Session barrier_;std::optional<p::Decision> decision_;
        std::uint64_t decisionStarted_{},connection_{},sent_{},received_{},retirementFixtures_{},entryFixtures_{},loadAuthorizations_{},timeoutDrain_{};
        bool retired_=false,hostEntry_=false,doneSent_=false,peerDone_=false,passed_=false;std::string failure_;
    };
}
int wmain(int argc,wchar_t** argv)
{try{return Exercise(Parse(argc,argv)).Run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
