#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include "LobbyDiscovery.h"
#include <array>
#include <algorithm>
#include <cstring>

namespace winxcoop::launcher
{
    namespace
    {
        constexpr std::size_t MaximumNameBytes = 48, ReplyHeader = 12;
        constexpr std::array<unsigned char,4> ProbeMagic{'W','C','Q',1}, ReplyMagic{'W','C','R',1};
        std::uint32_t Read32(const unsigned char* p) noexcept
        { return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24); }
        void Write32(unsigned char* p, std::uint32_t value) noexcept
        { for(unsigned i=0;i<4;++i) p[i]=static_cast<unsigned char>(value>>(i*8)); }
        sockaddr_in Address(const std::uint32_t ip, const std::uint16_t port) noexcept
        { sockaddr_in a{}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(ip); a.sin_port=htons(port); return a; }
        bool CleanName(const std::wstring& name) noexcept
        { return !name.empty() && std::all_of(name.begin(),name.end(),[](wchar_t c){return c>=32 && c!=127;}); }
    }

    struct LobbyDiscovery::Impl
    {
        SOCKET host=INVALID_SOCKET, search=INVALID_SOCKET;
        bool wsa=false, occupied=false;
        std::uint16_t tcpPort=0;
        std::array<char,MaximumNameBytes> name{};
        std::size_t nameSize=0;
        std::uint64_t started=0, nextProbe=0, lastNow=0, revision=0;
        std::uint32_t nonce=0;
        unsigned attempts=0;
        int error=0;
        std::vector<DiscoveredLobby> lobbies;
        DiscoveryMetrics metrics;

        bool StartRuntime()
        {
            if(wsa) return true;
            WSADATA data{}; error=WSAStartup(MAKEWORD(2,2),&data);
            if(error) return false;
            wsa=true; ++metrics.wsaStarts;
            if(data.wVersion!=MAKEWORD(2,2)) { error=WSAVERNOTSUPPORTED; Close(); return false; }
            return true;
        }
        void CloseSocket(SOCKET& socket) noexcept
        {
            if(socket==INVALID_SOCKET) return;
            if(closesocket(socket)==0) { socket=INVALID_SOCKET; ++metrics.socketsClosed; }
            else { error=WSAGetLastError(); ++metrics.closeErrors; }
        }
        bool Open(SOCKET& socket, const std::uint16_t port, const bool broadcast)
        {
            if(socket!=INVALID_SOCKET || !StartRuntime()) return false;
            socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            if(socket==INVALID_SOCKET) { error=WSAGetLastError(); return false; }
            ++metrics.socketsOpened;
            u_long nonblocking=1; const BOOL yes=TRUE;
            const auto address=Address(0,port);
            if(ioctlsocket(socket,FIONBIO,&nonblocking)==SOCKET_ERROR ||
               setsockopt(socket,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&yes),sizeof(yes))==SOCKET_ERROR ||
               (broadcast && setsockopt(socket,SOL_SOCKET,SO_BROADCAST,reinterpret_cast<const char*>(&yes),sizeof(yes))==SOCKET_ERROR) ||
               bind(socket,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==SOCKET_ERROR)
            { error=WSAGetLastError(); CloseSocket(socket); return false; }
            return true;
        }
        bool StartHosting(const std::uint16_t port, const std::wstring& title)
        {
            StopHosting();
            if(host!=INVALID_SOCKET) return false;
            if(!port || !CleanName(title) || title.size()>MaximumNameBytes) { error=WSAEINVAL; return false; }
            const auto converted=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,title.data(),static_cast<int>(title.size()),name.data(),static_cast<int>(name.size()),nullptr,nullptr);
            if(converted<=0) { error=WSAEINVAL; return false; }
            nameSize=static_cast<std::size_t>(converted); tcpPort=port;
            return Open(host,DiscoveryPort,false);
        }
        void StopHosting() noexcept { CloseSocket(host); tcpPort=0; nameSize=0; occupied=false; }
        bool BeginSearch(const std::uint64_t now)
        {
            CloseSocket(search); if(search!=INVALID_SOCKET) return false;
            lobbies.clear(); ++revision;
            if(!Open(search,0,true)) return false;
            started=lastNow=nextProbe=now; attempts=0;
            LARGE_INTEGER counter{}; QueryPerformanceCounter(&counter);
            nonce=static_cast<std::uint32_t>(counter.QuadPart) ^ static_cast<std::uint32_t>(now) ^ GetCurrentProcessId();
            if(!nonce) nonce=1; // A correlation value, not an authentication secret.
            return true;
        }
        bool Receive(SOCKET socket, std::array<unsigned char,256>& bytes, sockaddr_in& from, int& size)
        {
            int addressSize=sizeof(from);
            size=recvfrom(socket,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),0,reinterpret_cast<sockaddr*>(&from),&addressSize);
            if(size==SOCKET_ERROR)
            {
                const auto current=WSAGetLastError();
                if(current==WSAEWOULDBLOCK) return false;
                if(current==WSAEMSGSIZE) ++metrics.invalidDatagrams;
                else { error=current; ++metrics.receiveErrors; }
                size=-1; return true;
            }
            if(addressSize!=sizeof(from) || from.sin_family!=AF_INET) { ++metrics.invalidDatagrams; size=-1; }
            return true;
        }
        void PumpHost()
        {
            if(host==INVALID_SOCKET || !tcpPort) return;
            for(unsigned budget=0;budget<8;++budget)
            {
                std::array<unsigned char,256> bytes{}; sockaddr_in from{}; int size=0;
                if(!Receive(host,bytes,from,size)) break;
                if(size<0) continue;
                if(size!=8 || !std::equal(ProbeMagic.begin(),ProbeMagic.end(),bytes.begin()) || !Read32(bytes.data()+4) || !from.sin_addr.s_addr || !from.sin_port)
                { ++metrics.invalidDatagrams; continue; }
                ++metrics.probesReceived;
                std::array<unsigned char,ReplyHeader+MaximumNameBytes> reply{};
                std::copy(ReplyMagic.begin(),ReplyMagic.end(),reply.begin());
                std::copy_n(bytes.data()+4,4,reply.data()+4);
                reply[8]=static_cast<unsigned char>(tcpPort); reply[9]=static_cast<unsigned char>(tcpPort>>8);
                reply[10]=static_cast<unsigned char>(nameSize); reply[11]=occupied?1:0;
                std::memcpy(reply.data()+ReplyHeader,name.data(),nameSize);
                const auto sent=sendto(host,reinterpret_cast<const char*>(reply.data()),static_cast<int>(ReplyHeader+nameSize),0,reinterpret_cast<const sockaddr*>(&from),sizeof(from));
                if(sent==static_cast<int>(ReplyHeader+nameSize)) ++metrics.repliesSent;
                else { error=sent==SOCKET_ERROR?WSAGetLastError():WSAEFAULT; ++metrics.sendErrors; }
            }
        }
        void PumpSearch(const std::uint64_t now)
        {
            if(search==INVALID_SOCKET) return;
            if(now<lastNow || now-started>=SearchMilliseconds) { CloseSocket(search); ++revision; return; }
            lastNow=now;
            if(attempts<3 && now>=nextProbe)
            {
                std::array<unsigned char,8> probe{}; std::copy(ProbeMagic.begin(),ProbeMagic.end(),probe.begin()); Write32(probe.data()+4,nonce);
                const auto broadcast=Address(0xffffffffu,DiscoveryPort);
                const auto sent=sendto(search,reinterpret_cast<const char*>(probe.data()),static_cast<int>(probe.size()),0,reinterpret_cast<const sockaddr*>(&broadcast),sizeof(broadcast));
                if(sent==static_cast<int>(probe.size())) ++metrics.probesSent;
                else { error=sent==SOCKET_ERROR?WSAGetLastError():WSAEFAULT; ++metrics.sendErrors; }
                ++attempts; nextProbe=started+attempts*500;
            }
            for(unsigned budget=0;budget<16;++budget)
            {
                std::array<unsigned char,256> bytes{}; sockaddr_in from{}; int size=0;
                if(!Receive(search,bytes,from,size)) break;
                if(size<0) continue;
                const auto length=bytes[10]; const auto port=std::uint16_t(bytes[8] | (std::uint16_t(bytes[9])<<8));
                if(size<static_cast<int>(ReplyHeader) || size!=static_cast<int>(ReplyHeader+length) || length==0 || length>MaximumNameBytes || !port || bytes[11]>1 ||
                   !std::equal(ReplyMagic.begin(),ReplyMagic.end(),bytes.begin()) || Read32(bytes.data()+4)!=nonce || ntohs(from.sin_port)!=DiscoveryPort ||
                   !from.sin_addr.s_addr || from.sin_addr.s_addr==INADDR_BROADCAST)
                { ++metrics.invalidDatagrams; continue; }
                std::array<wchar_t,MaximumNameBytes> wide{};
                const auto chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(bytes.data()+ReplyHeader),length,wide.data(),static_cast<int>(wide.size()));
                if(chars<=0) { ++metrics.invalidDatagrams; continue; }
                std::wstring title(wide.data(),static_cast<std::size_t>(chars));
                if(!CleanName(title)) { ++metrics.invalidDatagrams; continue; }
                ++metrics.repliesReceived;
                const networking::Endpoint endpoint{ntohl(from.sin_addr.s_addr),port};
                auto found=std::find_if(lobbies.begin(),lobbies.end(),[&](const auto& x){return x.endpoint.address==endpoint.address && x.endpoint.port==endpoint.port;});
                if(found!=lobbies.end()) { found->name=std::move(title); found->occupied=bytes[11]!=0; ++revision; }
                else if(lobbies.size()<MaximumLobbies) { lobbies.push_back({endpoint,std::move(title),bytes[11]!=0}); ++revision; }
            }
        }
        void Close() noexcept
        {
            StopHosting(); CloseSocket(search);
            if(wsa && host==INVALID_SOCKET && search==INVALID_SOCKET)
            { wsa=false; ++metrics.wsaCleanups; if(WSACleanup()==SOCKET_ERROR) { error=WSAGetLastError(); ++metrics.wsaCleanupErrors; } }
        }
        ~Impl(){Close();}
    };

    LobbyDiscovery::LobbyDiscovery():impl_(std::make_unique<Impl>()){}
    LobbyDiscovery::~LobbyDiscovery()=default;
    bool LobbyDiscovery::StartHosting(const std::uint16_t port,const std::wstring& name){return impl_->StartHosting(port,name);}
    void LobbyDiscovery::StopHosting() noexcept {impl_->StopHosting();}
    void LobbyDiscovery::SetOccupied(const bool occupied) noexcept {impl_->occupied=occupied;}
    bool LobbyDiscovery::BeginSearch(const std::uint64_t now){return impl_->BeginSearch(now);}
    void LobbyDiscovery::Pump(const std::uint64_t now){impl_->PumpHost();impl_->PumpSearch(now);}
    void LobbyDiscovery::Close() noexcept {impl_->Close();}
    bool LobbyDiscovery::Hosting() const noexcept {return impl_->host!=INVALID_SOCKET;}
    bool LobbyDiscovery::Searching() const noexcept {return impl_->search!=INVALID_SOCKET;}
    int LobbyDiscovery::LastSocketError() const noexcept {return impl_->error;}
    std::uint64_t LobbyDiscovery::Revision() const noexcept {return impl_->revision;}
    const std::vector<DiscoveredLobby>& LobbyDiscovery::Lobbies() const noexcept {return impl_->lobbies;}
    const DiscoveryMetrics& LobbyDiscovery::Metrics() const noexcept {return impl_->metrics;}
}
