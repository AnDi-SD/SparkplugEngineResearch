#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "WindowsTcpTransport.h"
#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace winxcoop::networking
{
    namespace
    {
        sockaddr_in Address(const Endpoint endpoint) noexcept
        {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(endpoint.address);
            address.sin_port = htons(endpoint.port);
            return address;
        }
        Endpoint EndpointOf(const sockaddr_in& address) noexcept
        { return {ntohl(address.sin_addr.s_addr),ntohs(address.sin_port)}; }
        bool Terminal(const SessionCore& core) noexcept
        { return core.Phase()==SessionPhase::Closed || core.Phase()==SessionPhase::Failed; }
        bool PendingConnect(const int error) noexcept
        { return error==WSAEWOULDBLOCK || error==WSAEINPROGRESS || error==WSAEALREADY; }

        struct WinsockRuntime final
        {
            TcpTransportMetrics& metrics;
            bool active = false;
            explicit WinsockRuntime(TcpTransportMetrics& value):metrics(value){}
            void Start()
            {
                WSADATA data{};
                const auto error = WSAStartup(MAKEWORD(2,2),&data);
                if(error) throw std::system_error(error,std::system_category(),"WSAStartup");
                active = true; ++metrics.wsaStarts;
                if(data.wVersion!=MAKEWORD(2,2))
                { Stop(); throw std::runtime_error("Winsock 2.2 is unavailable"); }
            }
            void Stop() noexcept
            {
                if(!active) return;
                active=false; ++metrics.wsaCleanups;
                if(WSACleanup()==SOCKET_ERROR) ++metrics.wsaCleanupErrors;
            }
            ~WinsockRuntime(){Stop();}
        };
    }

    struct WindowsTcpTransport::Impl final
    {
        TcpTransportMetrics metrics;
        WinsockRuntime runtime{metrics};
        TcpTransportConfig config;
        SOCKET listener = INVALID_SOCKET, peer = INVALID_SOCKET, rejected = INVALID_SOCKET;
        TcpTransportState state = TcpTransportState::Idle;
        Endpoint bound;
        std::optional<Endpoint> remote;
        std::unique_ptr<SessionCore> core;
        std::optional<std::uint16_t> authorizedToken;
        std::uint64_t connectionCounter=0,activeConnection=0,connectStarted=0,lastNow=0,shutdownStarted=0;
        bool hasTime=false,writeShutdown=false;
        int lastSocketError=0;
        std::size_t ownedSockets=0;
        std::optional<TcpTerminal> terminal;
        std::vector<std::uint8_t> sending;
        std::size_t sendOffset=0;
        std::array<std::uint8_t,MaximumReceiveBuffer> receiving{};
        std::size_t receiveSize=0,receiveOffset=0;
        std::deque<TcpReceivedMessage> delivered;
        std::array<TcpSessionEvent,MaximumSessionEvents> events{};
        std::size_t eventHead=0,eventCount=0;
        std::optional<TcpSessionEvent> lastEvent;

        explicit Impl(TcpTransportConfig value):config(std::move(value)),bound(config.localBind)
        {
            if((config.role!=SessionRole::Host && config.role!=SessionRole::Guest) ||
               config.localBind.address==0xffffffffu || !config.heartbeatMilliseconds ||
               config.heartbeatMilliseconds>=config.timeoutMilliseconds || !config.connectTimeoutMilliseconds ||
               !config.maxSendChunk || config.maxSendChunk>MaximumReceiveBuffer ||
               !config.maxReceiveChunk || config.maxReceiveChunk>MaximumReceiveBuffer ||
               config.maxIoCallsPerPump<4 || config.maxIoCallsPerPump>4096 ||
               config.sendBufferBytes<0 || config.receiveBufferBytes<0)
                throw std::invalid_argument("Invalid TCP transport configuration");
            if((config.role==SessionRole::Host && config.server) ||
               (config.role==SessionRole::Guest && (!config.server || !config.server->address ||
                 config.server->address==0xffffffffu || !config.server->port)))
                throw std::invalid_argument("Guest requires a concrete server; Host owns its listener");
            runtime.Start();
        }
        ~Impl(){Close();}

        void SocketError(const int error) noexcept
        { lastSocketError=error; ++metrics.socketErrors; }
        SOCKET NewSocket() noexcept
        {
            const auto socketValue=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(socketValue==INVALID_SOCKET) { SocketError(WSAGetLastError()); return INVALID_SOCKET; }
            TrackSocket(); return socketValue;
        }
        void TrackSocket() noexcept
        {
            ++metrics.socketsOpened; ++ownedSockets;
            metrics.peakOwnedSockets=std::max(metrics.peakOwnedSockets,ownedSockets);
        }
        void CloseSocket(SOCKET& value,const bool abortive=false) noexcept
        {
            if(value==INVALID_SOCKET) return;
            if(abortive)
            {
                const linger option{1,0};
                if(setsockopt(value,SOL_SOCKET,SO_LINGER,reinterpret_cast<const char*>(&option),sizeof(option))==SOCKET_ERROR)
                    SocketError(WSAGetLastError());
            }
            auto result=closesocket(value);
            if(result==SOCKET_ERROR)
            {
                const auto error=WSAGetLastError(); ++metrics.socketCloseErrors; SocketError(error);
                // This adapter never enables timed linger. Explicitly undo it
                // before retrying if a provider nevertheless reports it.
                if(error==WSAEWOULDBLOCK)
                {
                    const linger option{0,0};
                    if(setsockopt(value,SOL_SOCKET,SO_LINGER,reinterpret_cast<const char*>(&option),sizeof(option))==0)
                        result=closesocket(value);
                }
            }
            if(result==0)
            { value=INVALID_SOCKET; --ownedSockets; ++metrics.socketsClosed; }
        }
        bool Options(const SOCKET value,const bool exclusiveBind) noexcept
        {
            u_long nonblocking=1;
            if(ioctlsocket(value,FIONBIO,&nonblocking)==SOCKET_ERROR)
            { SocketError(WSAGetLastError()); return false; }
            const BOOL enabled=TRUE;
            const linger noTimedLinger{0,0};
            if((exclusiveBind && setsockopt(value,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&enabled),sizeof(enabled))==SOCKET_ERROR) ||
               setsockopt(value,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&enabled),sizeof(enabled))==SOCKET_ERROR ||
               setsockopt(value,SOL_SOCKET,SO_LINGER,reinterpret_cast<const char*>(&noTimedLinger),sizeof(noTimedLinger))==SOCKET_ERROR ||
               (config.sendBufferBytes && setsockopt(value,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<const char*>(&config.sendBufferBytes),sizeof(config.sendBufferBytes))==SOCKET_ERROR) ||
               (config.receiveBufferBytes && setsockopt(value,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<const char*>(&config.receiveBufferBytes),sizeof(config.receiveBufferBytes))==SOCKET_ERROR))
            { SocketError(WSAGetLastError()); return false; }
            return true;
        }
        bool ReadEndpoint(const SOCKET value,const bool remoteEndpoint,Endpoint& output) noexcept
        {
            sockaddr_in address{}; int size=sizeof(address);
            const auto result=remoteEndpoint ? getpeername(value,reinterpret_cast<sockaddr*>(&address),&size)
                                             : getsockname(value,reinterpret_cast<sockaddr*>(&address),&size);
            if(result==SOCKET_ERROR) { SocketError(WSAGetLastError()); return false; }
            if(size<static_cast<int>(sizeof(address)) || address.sin_family!=AF_INET)
            { lastSocketError=0; return false; }
            output=EndpointOf(address); return true;
        }
        void Observe() noexcept
        {
            if(!core || !activeConnection) return;
            const TcpSessionEvent event{activeConnection,lastNow,core->Phase(),core->Error(),core->StartToken()};
            if(lastEvent && lastEvent->connectionId==event.connectionId &&
               lastEvent->sessionPhase==event.sessionPhase && lastEvent->sessionError==event.sessionError &&
               lastEvent->startToken==event.startToken) return;
            lastEvent=event;
            if(eventCount==MaximumSessionEvents)
            { eventHead=(eventHead+1)%MaximumSessionEvents; --eventCount; ++metrics.droppedSessionEvents; }
            events[(eventHead+eventCount)%MaximumSessionEvents]=event; ++eventCount;
        }
        void FinishPeer(const TcpCloseReason reason,const int error,const bool abortive=false) noexcept
        {
            if(peer==INVALID_SOCKET && !activeConnection) return;
            if(core && activeConnection)
            {
                if(reason==TcpCloseReason::RemoteEof) core->EndOfInput(lastNow);
                else if(reason==TcpCloseReason::LocalClose) core->Close();
                else core->FailTransport(); // First SessionError is preserved.
                Observe();
            }
            terminal=TcpTerminal{activeConnection,reason,error,
                core && activeConnection ? core->Phase() : SessionPhase::Idle,
                core && activeConnection ? core->Error() : SessionError::None};
            CloseSocket(peer,abortive);
            sending.clear(); sendOffset=0; receiveSize=receiveOffset=0; writeShutdown=false;
            activeConnection=0; remote.reset();
            if(peer!=INVALID_SOCKET) state=TcpTransportState::Failed;
            else if(config.role==SessionRole::Host && listener!=INVALID_SOCKET) state=TcpTransportState::Listening;
            else state=(reason==TcpCloseReason::LocalClose || reason==TcpCloseReason::RemoteEof) &&
                terminal->sessionPhase!=SessionPhase::Failed ? TcpTransportState::Closed : TcpTransportState::Failed;
        }
        void FailService(const TcpCloseReason reason,const int error) noexcept
        {
            if(peer!=INVALID_SOCKET || activeConnection) FinishPeer(reason,error);
            else terminal=TcpTerminal{0,reason,error,SessionPhase::Idle,SessionError::None};
            CloseSocket(listener); CloseSocket(rejected); state=TcpTransportState::Failed;
        }
        bool Time(const std::uint64_t now) noexcept
        {
            if(hasTime && now<lastNow)
            {
                if(core && activeConnection) { core->Tick(now); Observe(); }
                FailService(TcpCloseReason::ClockMovedBackward,0); return false;
            }
            hasTime=true; lastNow=now; return true;
        }
        bool Establish(const std::uint64_t now)
        {
            Endpoint local{},other{};
            if(!ReadEndpoint(peer,false,local) || !ReadEndpoint(peer,true,other))
            { FinishPeer(TcpCloseReason::SocketFailure,lastSocketError); return false; }
            try
            {
                SessionConfig sessionConfig;
                sessionConfig.role=config.role; sessionConfig.local=local;
                if(config.role==SessionRole::Guest) sessionConfig.server=other;
                sessionConfig.heartbeatMilliseconds=config.heartbeatMilliseconds;
                sessionConfig.timeoutMilliseconds=config.timeoutMilliseconds;
                auto next=std::make_unique<SessionCore>(sessionConfig);
                if(config.role==SessionRole::Host && authorizedToken)
                    (void)next->HostStart(*authorizedToken,now);
                const bool attached=next->Attach(now);
                core=std::move(next); activeConnection=++connectionCounter;
                if(!activeConnection) { FailService(TcpCloseReason::ResourceFailure,0); return false; }
                ++metrics.connections;
                if(config.role==SessionRole::Host) ++metrics.accepted;
                else bound=local;
                remote=other; state=TcpTransportState::Connected; Observe();
                if(!attached || Terminal(*core))
                { FinishPeer(TcpCloseReason::SessionFailure,0); return false; }
                return true;
            }
            catch(const std::bad_alloc&) { FinishPeer(TcpCloseReason::ResourceFailure,0); }
            catch(const std::exception&) { FinishPeer(TcpCloseReason::SessionFailure,0); }
            return false;
        }
        bool Start(const std::uint64_t now)
        {
            if(state!=TcpTransportState::Idle || !runtime.active || !Time(now)) return false;
            auto& socketValue=config.role==SessionRole::Host ? listener : peer;
            socketValue=NewSocket();
            if(socketValue==INVALID_SOCKET || !Options(socketValue,true))
            { FailService(TcpCloseReason::SocketFailure,lastSocketError); return false; }
            const auto address=Address(config.localBind);
            if(bind(socketValue,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==SOCKET_ERROR)
            { SocketError(WSAGetLastError()); FailService(TcpCloseReason::SocketFailure,lastSocketError); return false; }
            if(!ReadEndpoint(socketValue,false,bound))
            { FailService(TcpCloseReason::SocketFailure,lastSocketError); return false; }
            if(config.role==SessionRole::Host)
            {
                if(listen(listener,4)==SOCKET_ERROR)
                { SocketError(WSAGetLastError()); FailService(TcpCloseReason::SocketFailure,lastSocketError); return false; }
                state=TcpTransportState::Listening; return true;
            }
            const auto server=Address(*config.server);
            if(connect(peer,reinterpret_cast<const sockaddr*>(&server),sizeof(server))==0) return Establish(now);
            const auto error=WSAGetLastError();
            if(PendingConnect(error)) { connectStarted=now; state=TcpTransportState::Connecting; return true; }
            SocketError(error); FinishPeer(TcpCloseReason::ConnectFailure,error); return false;
        }
        void Accept(const std::uint64_t now,std::size_t& budget)
        {
            if(listener==INVALID_SOCKET || !budget) return;
            --budget;
            const auto incoming=accept(listener,nullptr,nullptr);
            if(incoming==INVALID_SOCKET)
            {
                const auto error=WSAGetLastError();
                if(error!=WSAEWOULDBLOCK && error!=WSAEINTR)
                { SocketError(error); FailService(TcpCloseReason::SocketFailure,error); }
                return;
            }
            TrackSocket();
            if(peer!=INVALID_SOCKET)
            {
                rejected=incoming; ++metrics.refused; CloseSocket(rejected,true);
                if(rejected!=INVALID_SOCKET) { FailService(TcpCloseReason::SocketFailure,lastSocketError); }
                return;
            }
            peer=incoming;
            if(!Options(peer,false)) { FinishPeer(TcpCloseReason::SocketFailure,lastSocketError); return; }
            (void)Establish(now);
        }
        void ConnectReady(const std::uint64_t now,std::size_t& budget)
        {
            if(now-connectStarted>=config.connectTimeoutMilliseconds)
            { FinishPeer(TcpCloseReason::ConnectTimeout,0); return; }
            if(!budget) return;
            --budget;
            fd_set writable{},exceptional{}; FD_ZERO(&writable); FD_ZERO(&exceptional);
            FD_SET(peer,&writable); FD_SET(peer,&exceptional); timeval wait{};
            const auto ready=select(0,nullptr,&writable,&exceptional,&wait);
            if(ready==SOCKET_ERROR)
            {
                const auto error=WSAGetLastError();
                if(error!=WSAEINTR) { SocketError(error); FinishPeer(TcpCloseReason::ConnectFailure,error); }
                return;
            }
            if(!ready) return;
            int error=0,size=sizeof(error);
            if(getsockopt(peer,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&size)==SOCKET_ERROR)
            { SocketError(WSAGetLastError()); FinishPeer(TcpCloseReason::ConnectFailure,lastSocketError); return; }
            if(error) { SocketError(error); FinishPeer(TcpCloseReason::ConnectFailure,error); return; }
            (void)Establish(now);
        }
        bool CheckSession() noexcept
        {
            if(core && activeConnection && Terminal(*core))
            { FinishPeer(TcpCloseReason::SessionFailure,0); return false; }
            return state==TcpTransportState::Connected;
        }
        void FlushSend(std::size_t& budget)
        {
            while(budget && state==TcpTransportState::Connected && !writeShutdown)
            {
                if(sending.empty())
                {
                    auto frame=core->TakeOutbound();
                    if(!frame) return;
                    sending=std::move(*frame); sendOffset=0;
                }
                const auto remaining=sending.size()-sendOffset;
                const auto request=std::min(remaining,config.maxSendChunk);
                if(request<remaining) ++metrics.sendRequestCapped;
                --budget; ++metrics.sendCalls;
                const auto result=send(peer,reinterpret_cast<const char*>(sending.data()+sendOffset),static_cast<int>(request),0);
                if(result>0)
                {
                    metrics.sentBytes+=static_cast<unsigned>(result);
                    if(static_cast<std::size_t>(result)<request) ++metrics.nativeShortSendResults;
                    sendOffset+=static_cast<std::size_t>(result);
                    if(sendOffset<sending.size()) ++metrics.framePartialAdvances;
                    else { sending.clear(); sendOffset=0; }
                    continue;
                }
                if(result==SOCKET_ERROR)
                {
                    const auto error=WSAGetLastError();
                    if(error==WSAEWOULDBLOCK) { ++metrics.sendWouldBlock; return; }
                    if(error==WSAEINTR) continue;
                    SocketError(error); FinishPeer(TcpCloseReason::SocketFailure,error); return;
                }
                // Positive-length send returning zero makes no progress; its
                // lack of an OS error code is preserved rather than invented.
                FinishPeer(TcpCloseReason::SocketFailure,0); return;
            }
        }
        bool DrainReceived(const std::uint64_t now)
        {
            while(receiveOffset<receiveSize && state==TcpTransportState::Connected)
            {
                if(delivered.size()==MaximumDeliveredMessages)
                { ++metrics.receivePausedForInbox; return false; }
                // At most one full packet can complete in a HeaderSize slice.
                // Drain that message before processing any later EOF/error or
                // malformed packet in the same positive OS receive chunk.
                const auto amount=std::min(TcpPacketFramer::HeaderSize,receiveSize-receiveOffset);
                const auto consumed=core->Receive(receiving.data()+receiveOffset,amount,now);
                receiveOffset+=consumed; Observe();
                while(auto message=core->TakeMessage())
                {
                    if(delivered.size()==MaximumDeliveredMessages)
                    { FinishPeer(TcpCloseReason::ResourceFailure,0); return false; }
                    delivered.push_back({activeConnection,std::move(*message)});
                }
                if(!CheckSession()) return false;
                if(consumed!=amount) { FinishPeer(TcpCloseReason::SessionFailure,0); return false; }
            }
            if(receiveOffset==receiveSize) receiveOffset=receiveSize=0;
            return state==TcpTransportState::Connected;
        }
        void ReceiveData(const std::uint64_t now,std::size_t& budget)
        {
            if(!DrainReceived(now)) return;
            while(budget && state==TcpTransportState::Connected)
            {
                if(delivered.size()==MaximumDeliveredMessages)
                { ++metrics.receivePausedForInbox; return; }
                --budget; ++metrics.receiveCalls;
                const auto result=recv(peer,reinterpret_cast<char*>(receiving.data()),static_cast<int>(config.maxReceiveChunk),0);
                if(result>0)
                {
                    metrics.receivedBytes+=static_cast<unsigned>(result);
                    receiveOffset=0; receiveSize=static_cast<std::size_t>(result);
                    if(!DrainReceived(now)) return;
                    continue;
                }
                if(result==0) { ++metrics.remoteEofs; FinishPeer(TcpCloseReason::RemoteEof,0); return; }
                const auto error=WSAGetLastError();
                if(error==WSAEWOULDBLOCK) { ++metrics.receiveWouldBlock; return; }
                if(error==WSAEINTR) continue;
                SocketError(error); FinishPeer(TcpCloseReason::SocketFailure,error); return;
            }
        }
        void Pump(const std::uint64_t now)
        {
            if(state==TcpTransportState::Idle || state==TcpTransportState::Closed || state==TcpTransportState::Failed || !Time(now)) return;
            try
            {
                std::size_t budget=config.maxIoCallsPerPump;
                // One accept per pump bounds refusal work without starving an
                // existing connection; data quotas reserve both directions.
                if(config.role==SessionRole::Host) Accept(now,budget);
                else if(state==TcpTransportState::Connecting) ConnectReady(now,budget);
                if(state!=TcpTransportState::Connected) return;
                if(writeShutdown)
                {
                    if(now-shutdownStarted>config.timeoutMilliseconds)
                    { FinishPeer(TcpCloseReason::ShutdownTimeout,0); return; }
                }
                else core->Tick(now);
                Observe(); if(!CheckSession()) return;
                std::size_t sends=budget/2,receives=budget-sends;
                FlushSend(sends);
                if(state==TcpTransportState::Connected) ReceiveData(now,receives);
                if(state==TcpTransportState::Connected)
                { sends+=receives; FlushSend(sends); }
            }
            catch(const std::bad_alloc&) { FinishPeer(TcpCloseReason::ResourceFailure,0); }
            catch(const std::exception&) { FinishPeer(TcpCloseReason::SessionFailure,0); }
        }
        bool HostStart(const std::uint16_t token,const std::uint64_t now)
        {
            if(config.role!=SessionRole::Host || !token || writeShutdown || state==TcpTransportState::Closed || state==TcpTransportState::Failed || !Time(now)) return false;
            if(authorizedToken && *authorizedToken!=token) return false;
            if(activeConnection)
            {
                const bool result=core->HostStart(token,now); Observe();
                if(!CheckSession() || !result) return false;
            }
            authorizedToken=token; return true;
        }
        bool SendApplicationMessage(const std::uint64_t expected,const std::uint32_t classId,const std::uint32_t key,
            const void* data,const std::uint16_t size,const std::uint64_t now)
        {
            if(!expected || expected!=activeConnection || state!=TcpTransportState::Connected || writeShutdown) return false;
            if(size>sparkplug::reconstruction::spNetworkPacket::PayloadCapacity || (size && !data))
                throw std::invalid_argument("Invalid TCP application message pointer/size");
            if(!Time(now)) return false;
            core->Tick(now); Observe(); if(!CheckSession()) return false;
            if(core->OutboundCount()>=ApplicationQueueLimit)
            { ++metrics.applicationSendBackpressure; return false; }
            const bool result=core->SendApplicationMessage(classId,key,data,size,now); Observe();
            (void)CheckSession(); return result;
        }
        bool ShutdownSend() noexcept
        {
            if(state!=TcpTransportState::Connected || writeShutdown || !sending.empty() || core->OutboundCount()) return false;
            if(shutdown(peer,SD_SEND)==SOCKET_ERROR)
            { SocketError(WSAGetLastError()); FinishPeer(TcpCloseReason::SocketFailure,lastSocketError); return false; }
            writeShutdown=true; shutdownStarted=lastNow; return true;
        }
        void Close() noexcept
        {
            if(peer!=INVALID_SOCKET || activeConnection) FinishPeer(TcpCloseReason::LocalClose,0);
            CloseSocket(listener); CloseSocket(rejected);
            if(!ownedSockets) { runtime.Stop(); state=TcpTransportState::Closed; }
            else state=TcpTransportState::Failed;
        }
    };

    WindowsTcpTransport::WindowsTcpTransport(TcpTransportConfig config):impl_(std::make_unique<Impl>(std::move(config))){}
    WindowsTcpTransport::~WindowsTcpTransport()=default;
    bool WindowsTcpTransport::Start(const std::uint64_t now){return impl_->Start(now);}
    void WindowsTcpTransport::Pump(const std::uint64_t now){impl_->Pump(now);}
    bool WindowsTcpTransport::HostStart(const std::uint16_t token,const std::uint64_t now){return impl_->HostStart(token,now);}
    bool WindowsTcpTransport::SendApplicationMessage(const std::uint64_t expected,const std::uint32_t classId,const std::uint32_t key,
        const void* data,const std::uint16_t size,const std::uint64_t now)
    { return impl_->SendApplicationMessage(expected,classId,key,data,size,now); }
    std::optional<TcpReceivedMessage> WindowsTcpTransport::TakeMessage()
    {
        if(impl_->delivered.empty()) return std::nullopt;
        auto value=std::move(impl_->delivered.front()); impl_->delivered.pop_front(); return value;
    }
    std::optional<TcpSessionEvent> WindowsTcpTransport::TakeSessionEvent() noexcept
    {
        if(!impl_->eventCount) return std::nullopt;
        const auto value=impl_->events[impl_->eventHead];
        impl_->eventHead=(impl_->eventHead+1)%MaximumSessionEvents; --impl_->eventCount; return value;
    }
    bool WindowsTcpTransport::ShutdownSend(){return impl_->ShutdownSend();}
    void WindowsTcpTransport::ClosePeer(const bool abortive) noexcept{impl_->FinishPeer(TcpCloseReason::LocalClose,0,abortive);}
    void WindowsTcpTransport::Close() noexcept{impl_->Close();}
    TcpTransportState WindowsTcpTransport::State() const noexcept{return impl_->state;}
    Endpoint WindowsTcpTransport::BoundEndpoint() const noexcept{return impl_->bound;}
    std::optional<Endpoint> WindowsTcpTransport::PeerEndpoint() const noexcept{return impl_->remote;}
    const SessionCore* WindowsTcpTransport::Session() const noexcept{return impl_->core.get();}
    std::uint64_t WindowsTcpTransport::ActiveConnectionId() const noexcept{return impl_->activeConnection;}
    std::optional<TcpTerminal> WindowsTcpTransport::LastTerminal() const noexcept{return impl_->terminal;}
    int WindowsTcpTransport::LastSocketError() const noexcept{return impl_->lastSocketError;}
    const TcpTransportMetrics& WindowsTcpTransport::Metrics() const noexcept{return impl_->metrics;}
    std::size_t WindowsTcpTransport::OwnedSocketCount() const noexcept{return impl_->ownedSockets;}
    std::size_t WindowsTcpTransport::PendingSendBytes() const noexcept{return impl_->sending.size()-impl_->sendOffset;}
    std::size_t WindowsTcpTransport::PendingReceiveBytes() const noexcept{return impl_->receiveSize-impl_->receiveOffset;}
    std::size_t WindowsTcpTransport::DeliveredMessageCount() const noexcept{return impl_->delivered.size();}
}
