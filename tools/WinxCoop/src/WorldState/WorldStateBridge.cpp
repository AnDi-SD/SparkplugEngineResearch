// Isolated WorldState1 bridge. One owning thread; no native game calls or UI.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "WorldStateSession.h"
#include "Networking/WindowsTcpTransport.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

using namespace winxcoop::world_state;
using namespace winxcoop::networking;
namespace
{
    std::uint64_t Number(const std::wstring& value,std::uint64_t maximum)
    {
        std::string ascii;for(auto c:value){if(c<L'0'||c>L'9')throw std::invalid_argument("Expected decimal integer");ascii+=static_cast<char>(c);}
        std::uint64_t result=0;const auto parsed=std::from_chars(ascii.data(),ascii.data()+ascii.size(),result);
        if(ascii.empty()||parsed.ec!=std::errc{}||parsed.ptr!=ascii.data()+ascii.size()||result>maximum)throw std::invalid_argument("Integer outsideWorldState bound");return result;
    }
    Endpoint ParseEndpoint(const std::wstring& value)
    {
        const auto colon=value.rfind(L':');if(colon==std::wstring::npos)throw std::invalid_argument("IPv4:port required");
        const auto ip=value.substr(0,colon);std::size_t begin=0;std::uint32_t address=0;
        for(unsigned i=0;i<4;++i)
        {
            const auto end=i==3?ip.size():ip.find(L'.',begin);
            if(end==std::wstring::npos||end<=begin)throw std::invalid_argument("InvalidIPv4");
            address=(address<<8)|static_cast<std::uint32_t>(Number(ip.substr(begin,end-begin),255));begin=end+1;
        }
        return {address,static_cast<std::uint16_t>(Number(value.substr(colon+1),65535))};
    }
    std::uint64_t Creation(HANDLE process)
    {
        FILETIME c{},e{},k{},u{};if(!GetProcessTimes(process,&c,&e,&k,&u))throw std::runtime_error("Process creation query failed");
        return(std::uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime;
    }
    void FreshFile(const std::filesystem::path& path,const std::string& data)
    {
        const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Fresh WorldState output required");
        DWORD written=0;const bool ok=data.size()<=MAXDWORD&&WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)&&written==data.size();
        const bool closed=CloseHandle(file)!=0;if(!ok||!closed)throw std::runtime_error("WorldState output write/close failed");
    }
    struct Options final
    {
        Role role=Role::Host;TcpTransportConfig tcp;std::wstring mapping;std::filesystem::path ready,report;
        std::uint64_t nonce=0,session=0,scenario=0,connectionToken=0,runMilliseconds=60000;
        std::uint32_t levelEpoch=0,generation=0,actorLeaseMilliseconds=DefaultActorLeaseMilliseconds;
        std::uint16_t startToken=0xE501;
    };
    Options Parse(int argc,wchar_t** argv)
    {
        Options o;bool role=false,bind=false;std::vector<std::wstring> seen;
        for(int i=1;i<argc;++i)
        {
            const std::wstring key=argv[i];if(std::find(seen.begin(),seen.end(),key)!=seen.end()||i+1>=argc)throw std::invalid_argument("Unique key/value options required");
            seen.push_back(key);const std::wstring value=argv[++i];
            if(key==L"--role") {if(value!=L"host"&&value!=L"guest")throw std::invalid_argument("Role is host or guest");o.role=value==L"host"?Role::Host:Role::Guest;o.tcp.role=o.role==Role::Host?SessionRole::Host:SessionRole::Guest;role=true;}
            else if(key==L"--mapping")o.mapping=value;
            else if(key==L"--bind"){o.tcp.localBind=ParseEndpoint(value);bind=true;}
            else if(key==L"--server")o.tcp.server=ParseEndpoint(value);
            else if(key==L"--nonce")o.nonce=Number(value,UINT64_MAX);
            else if(key==L"--session")o.session=Number(value,UINT64_MAX);
            else if(key==L"--scenario")o.scenario=Number(value,UINT64_MAX);
            else if(key==L"--connection-token")o.connectionToken=Number(value,UINT64_MAX);
            else if(key==L"--level-epoch")o.levelEpoch=static_cast<std::uint32_t>(Number(value,UINT32_MAX));
            else if(key==L"--generation")o.generation=static_cast<std::uint32_t>(Number(value,UINT32_MAX));
            else if(key==L"--run-ms")o.runMilliseconds=Number(value,600000);
            else if(key==L"--actor-lease-ms")o.actorLeaseMilliseconds=static_cast<std::uint32_t>(Number(value,5000));
            else if(key==L"--token")o.startToken=static_cast<std::uint16_t>(Number(value,65535));
            else if(key==L"--ready-file")o.ready=value;
            else if(key==L"--report")o.report=value;
            else throw std::invalid_argument("UnknownWorldState option");
        }
        const std::wstring prefix=L"Local\\WinxCoop.WorldState1.";
        if(!role||!bind||!o.nonce||!o.session||!o.scenario||!o.connectionToken||!o.levelEpoch||!o.generation||!o.startToken||
            o.runMilliseconds<100||o.actorLeaseMilliseconds<500||(o.role==Role::Guest)!=o.tcp.server.has_value()||
            o.mapping.size()<=prefix.size()||o.mapping.size()>160||o.mapping.compare(0,prefix.size(),prefix)!=0||o.ready.empty()||o.report.empty()||
            !o.ready.is_absolute()||!o.report.is_absolute()||o.ready==o.report||std::filesystem::exists(o.ready)||std::filesystem::exists(o.report))
            throw std::invalid_argument("Missing explicit freshWorldState identity, output or endpoint");
        for(std::size_t i=prefix.size();i<o.mapping.size();++i)
        {const auto c=o.mapping[i];if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'||c==L'_'))throw std::invalid_argument("Mapping suffix is not bounded ASCII");}
        o.tcp.heartbeatMilliseconds=1000;o.tcp.timeoutMilliseconds=10000;return o;
    }
    class Mapping final
    {
        HANDLE handle_=nullptr;Shared* data_=nullptr;DWORD closeError_=0;bool created_=false,closed_=false;
    public:
        explicit Mapping(const Options& o)
        {
            const auto creation=Creation(GetCurrentProcess());
            handle_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),o.mapping.c_str());
            if(!handle_)throw std::runtime_error("Cannot createWorldState mapping");
            if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(handle_);handle_=nullptr;throw std::runtime_error("WorldState mapping must be fresh");}
            data_=static_cast<Shared*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
            if(!data_){CloseHandle(handle_);handle_=nullptr;throw std::runtime_error("Cannot mapWorldState");}
            new(data_)Shared{};created_=true;
            data_->header={IpcMagic,IpcVersion,sizeof(Shared),MaximumSnapshotBytes,o.nonce,GetCurrentProcessId(),static_cast<unsigned>(o.role),creation,o.session,o.scenario,o.levelEpoch,o.generation};MemoryBarrier();
        }
        ~Mapping(){Close();}Mapping(const Mapping&)=delete;Mapping& operator=(const Mapping&)=delete;
        Shared& Get()noexcept{return *data_;}
        void Close()noexcept
        {
            if(data_){if(!UnmapViewOfFile(data_))closeError_=GetLastError();data_=nullptr;}
            if(handle_){if(!CloseHandle(handle_))closeError_=GetLastError();handle_=nullptr;}
            if(created_&&!closeError_)closed_=true;
        }
        bool Balanced()const noexcept{return closed_&&!closeError_;}
        DWORD Error()const noexcept{return closeError_;}
    };
    class Bridge final
    {
        Options o_;Mapping mapping_;Shared& ipc_;Header header_;Session world_;WindowsTcpTransport tcp_;
        ActorStatus actor_{};BridgeStatus status_{};HANDLE participant_=nullptr;bool admitted_=false,running_=false;
        std::uint64_t connection_=0,lastNow_=0;std::uint32_t snapshotPublication_=0;
        std::uint64_t sentFrames_=0,receivedFrames_=0,snapshotsForwarded_=0,snapshotReads_=0,coherentRetries_=0;
        DWORD participantCloseError_=0;
        bool Host()const noexcept{return o_.role==Role::Host;}
        void Stop(Terminal why)noexcept{world_.Stop(why);}
        bool ReadActor(std::uint64_t now)
        {
            if(admitted_)
            {
                if(WaitForSingleObject(participant_,0)!=WAIT_TIMEOUT){Stop(Terminal::ActorClosed);return false;}
                if(actor_.observedAtMilliseconds>now){Stop(Terminal::Clock);return false;}
            }
            ActorStatus next{};if(!ReadCoherent(ipc_.actor,next))
            {
                ++coherentRetries_;
                // A stalled writer is still bounded, but an old cached
                // observation must not reject a newer coherent publication.
                if(admitted_&&actor_.availability==static_cast<unsigned>(Availability::Ready)&&
                    now-actor_.observedAtMilliseconds>o_.actorLeaseMilliseconds)Stop(Terminal::ActorLeaseExpired);
                return false;
            }
            if(!next.participantPid)return false;
            const auto after=GetTickCount64();
            if(after<now||next.observedAtMilliseconds>after){Stop(Terminal::Clock);return false;}
            if(!next.participantCreationFileTime||!next.readinessRevision||next.reserved1||next.reserved2||
                next.availability>static_cast<unsigned>(Availability::Retired)||next.terminalReason>static_cast<unsigned>(Terminal::NativeApply)||
                (Host()&&(next.appliedEpoch||next.appliedRevision))) {Stop(Terminal::Identity);return false;}
            if(!admitted_)
            {
                participant_=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,next.participantPid);
                if(!participant_||Creation(participant_)!=next.participantCreationFileTime){Stop(Terminal::Identity);return false;}
                admitted_=true;
            }
            else if(next.participantPid!=actor_.participantPid||next.participantCreationFileTime!=actor_.participantCreationFileTime||
                next.readinessRevision<actor_.readinessRevision||next.observedAtMilliseconds<actor_.observedAtMilliseconds)
            {Stop(Terminal::Identity);return false;}
            actor_=next;
            if(WaitForSingleObject(participant_,0)!=WAIT_TIMEOUT){Stop(Terminal::ActorClosed);return false;}
            if(actor_.terminalReason){Stop(static_cast<Terminal>(actor_.terminalReason));return false;}
            if(actor_.availability==static_cast<unsigned>(Availability::Ready)&&after-actor_.observedAtMilliseconds>o_.actorLeaseMilliseconds)
            {Stop(Terminal::ActorLeaseExpired);return false;}
            if(!world_.ObserveActor(static_cast<Availability>(actor_.availability),actor_.readinessRevision))return false;
            if(!Host()&&!world_.ObserveApplied(actor_.appliedEpoch,actor_.appliedRevision))return false;return true;
        }
        void ReadHostSnapshot()
        {
            const auto publication=Acquire(ipc_.snapshot.publication);
            if(!publication||publication==snapshotPublication_)return;
            auto copy=std::make_unique<SnapshotSlot>();if(!ReadCoherent(ipc_.snapshot,*copy)){++coherentRetries_;return;}
            if(!copy->bytes)return;
            if(copy->bytes>MaximumSnapshotBytes||copy->reserved0||std::any_of(std::begin(copy->reserved),std::end(copy->reserved),[](auto x){return x!=0;}))
            {Stop(Terminal::InvalidRecord);return;}
            auto snapshot=DecodeSnapshot(copy->payload,copy->bytes);
            if(!snapshot||snapshot->revision!=copy->revision||snapshot->nativeTick!=copy->nativeTick||snapshot->flags!=copy->flags)
            {Stop(Terminal::InvalidRecord);return;}
            if(world_.OfferSnapshot(*snapshot)){snapshotPublication_=copy->publication;++snapshotReads_;}
        }
        void PublishGuestSnapshot()
        {
            auto snapshot=world_.TakeReceivedSnapshot();if(!snapshot)return;
            auto bytes=EncodeSnapshot(*snapshot);if(!bytes){Stop(Terminal::InvalidRecord);return;}
            auto slot=std::make_unique<SnapshotSlot>();slot->bytes=static_cast<unsigned>(bytes->size());slot->revision=snapshot->revision;slot->nativeTick=snapshot->nativeTick;slot->flags=snapshot->flags;
            std::memcpy(slot->payload,bytes->data(),bytes->size());
            if(!PublishCoherent(ipc_.snapshot,*slot)){Stop(Terminal::CounterExhausted);return;}++snapshotsForwarded_;
        }
        void Publish(std::uint64_t now)
        {
            status_.phase=static_cast<unsigned>(world_.State());status_.connectionToken=world_.Token();status_.syncEpoch=world_.Epoch();
            status_.terminalReason=static_cast<unsigned>(world_.Failure());status_.sentRevision=world_.SentRevision();status_.appliedRevision=world_.AppliedRevision();
            status_.bridgeTickMilliseconds=now;status_.receivedRevision=world_.ReceivedRevision();
            if(!PublishCoherent(ipc_.bridge,status_))Stop(Terminal::CounterExhausted);
        }
        bool peerStopReceived_=false, stopFlushed_=true;
        void Work(std::uint64_t now)
        {
            while(const auto message=tcp_.TakeMessage())
            {
                if(message->connectionId!=connection_){Stop(Terminal::Identity);return;}
                const auto decoded=Decode(message->message);
                if(decoded && decoded->kind==Kind::Stop)peerStopReceived_=true;
                if(!decoded||!world_.Receive(*decoded)){if(world_.Failure()==Terminal::None)Stop(Terminal::InvalidRecord);return;}++receivedFrames_;
            }
            if(Host())ReadHostSnapshot();else PublishGuestSnapshot();
            if(world_.Failure()!=Terminal::None)return;
            for(std::size_t i=0;i<WindowsTcpTransport::ApplicationQueueLimit;++i)
            {
                const auto next=world_.PeekOutbound();if(!next)break;
                const auto bytes=Encode(*next);if(!bytes){Stop(Terminal::InvalidRecord);return;}
                if(!tcp_.SendApplicationMessage(connection_,ApplicationClass,static_cast<unsigned>(next->kind),bytes->data(),static_cast<std::uint16_t>(bytes->size()),now))break;
                if(!world_.CommitOutbound())return;++sentFrames_;
            }
        }
    public:
        explicit Bridge(Options options):o_(std::move(options)),mapping_(o_),ipc_(mapping_.Get()),header_(ipc_.header),
            world_(o_.role,{o_.session,o_.scenario,o_.connectionToken,o_.levelEpoch,o_.generation}),tcp_(o_.tcp){}
        ~Bridge(){if(participant_)CloseHandle(participant_);}
        int Run()try
        {
            const auto begin=GetTickCount64();lastNow_=begin;Publish(begin);
            if(!tcp_.Start(begin)||(Host()&&!tcp_.HostStart(o_.startToken,begin)))Stop(Terminal::Transport);
            const auto bound=tcp_.BoundEndpoint();
            FreshFile(o_.ready,"{\"pid\":"+std::to_string(header_.ownerPid)+",\"creationFileTimeDecimal\":\""+std::to_string(header_.ownerCreationFileTime)+
                "\",\"nonceDecimal\":\""+std::to_string(header_.nonce)+"\",\"ipcBytes\":"+std::to_string(sizeof(Shared))+",\"port\":"+std::to_string(bound.port)+",\"worldStateVersion\":1}\n");
            while(world_.Failure()==Terminal::None)
            {
                const auto now=GetTickCount64();if(now<lastNow_){Stop(Terminal::Clock);break;}lastNow_=now;
                if(now-begin>=o_.runMilliseconds){Stop(Terminal::RunLimit);break;}
                if(std::memcmp(&ipc_.header,&header_,sizeof(Header))){Stop(Terminal::Identity);break;}
                (void)ReadActor(now);if(world_.Failure()!=Terminal::None)break;
                tcp_.Pump(now);const auto active=tcp_.ActiveConnectionId();
                if(active){if(connection_&&active!=connection_){Stop(Terminal::Identity);break;}connection_=active;}
                running_=tcp_.Session()&&tcp_.Session()->Phase()==SessionPhase::Running;
                // Drain already delivered positive receive chunks before EOF.
                // The final chunk may contain the peer's normal Stop record.
                if(running_||connection_)Work(now);
                if(world_.Failure()==Terminal::None && (tcp_.State()==TcpTransportState::Closed||tcp_.State()==TcpTransportState::Failed||(connection_&&!active)))
                    Stop(Terminal::PeerClosed);
                Publish(now);if(world_.Failure()==Terminal::None)Sleep(1);
            }
            const auto end=GetTickCount64();Publish(end);
            if(connection_&&running_&&world_.Token()&&!peerStopReceived_)
            {
                Message stop;stop.kind=Kind::Stop;stop.sessionId=o_.session;stop.scenarioId=o_.scenario;stop.connectionToken=world_.Token();
                stop.levelEpoch=o_.levelEpoch;stop.generation=o_.generation;stop.syncEpoch=world_.Epoch();stop.terminalReason=static_cast<unsigned>(world_.Failure());
                const auto bytes=Encode(stop);bool accepted=false;stopFlushed_=false;
                for(auto now=end;bytes&&now-end<750;now=GetTickCount64())
                {
                    if(!accepted)accepted=tcp_.SendApplicationMessage(connection_,ApplicationClass,static_cast<unsigned>(Kind::Stop),bytes->data(),static_cast<std::uint16_t>(bytes->size()),now);
                    tcp_.Pump(now);
                    if(accepted&&tcp_.PendingSendBytes()==0){stopFlushed_=true;break;}
                    if(tcp_.State()==TcpTransportState::Closed||tcp_.State()==TcpTransportState::Failed)break;
                    Sleep(1);
                }
            }
            tcp_.Close();if(participant_){if(!CloseHandle(participant_))participantCloseError_=GetLastError();participant_=nullptr;}mapping_.Close();
            const auto& metrics=tcp_.Metrics();
            const auto report="{\"worldStateVersion\":1,\"terminalReason\":"+std::to_string(static_cast<unsigned>(world_.Failure()))+
                ",\"syncEpoch\":"+std::to_string(world_.Epoch())+",\"sentRevisionDecimal\":\""+std::to_string(world_.SentRevision())+"\",\"appliedRevisionDecimal\":\""+std::to_string(world_.AppliedRevision())+
                "\",\"receivedRevisionDecimal\":\""+std::to_string(world_.ReceivedRevision())+"\",\"sentFrames\":"+std::to_string(sentFrames_)+",\"receivedFrames\":"+std::to_string(receivedFrames_)+
                ",\"snapshotReads\":"+std::to_string(snapshotReads_)+",\"snapshotsForwarded\":"+std::to_string(snapshotsForwarded_)+",\"coalescedUnpublished\":"+std::to_string(world_.CoalescedUnpublished())+
                ",\"nativeAppliedSnapshotCount\":"+std::to_string(world_.AppliedSnapshotCount())+",\"coherentReadRetries\":"+std::to_string(coherentRetries_)+",\"actorLeaseMillis\":"+std::to_string(o_.actorLeaseMilliseconds)+
                ",\"socketsOpened\":"+std::to_string(metrics.socketsOpened)+",\"socketsClosed\":"+std::to_string(metrics.socketsClosed)+
                ",\"stopFlushed\":"+(stopFlushed_?"true":"false")+",\"peerStopReceived\":"+(peerStopReceived_?"true":"false")+
                ",\"mappingClosed\":"+(mapping_.Balanced()?"true":"false")+",\"mappingCloseError\":"+std::to_string(mapping_.Error())+",\"participantCloseError\":"+std::to_string(participantCloseError_)+
                ",\"wireCodec\":\"existing-SessionCore-Packet-MemoryStream\",\"snapshotDataAgePolicy\":\"absolute-latest; actor-lease-separate\",\"gameplayQualified\":false}\n";
            FreshFile(o_.report,report);std::cout<<report;
            return stopFlushed_&&mapping_.Balanced()&&!participantCloseError_&&metrics.socketsOpened==metrics.socketsClosed&&
                (world_.Failure()==Terminal::ActorClosed||world_.Failure()==Terminal::RunLimit)?0:2;
        }
        catch(...){Stop(Terminal::InvalidRecord);Publish(GetTickCount64());tcp_.Close();throw;}
    };
}
int wmain(int argc,wchar_t** argv)
{
    try{Bridge bridge(Parse(argc,argv));return bridge.Run();}
    catch(const std::exception& e){std::cerr<<"WorldStateBridge: "<<e.what()<<'\n';return 1;}
}
