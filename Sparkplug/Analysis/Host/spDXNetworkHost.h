#pragma once
// Our platform boundary. These services describe imported WinSock/CRT calls,
// not a recovered backend, and never supply an implicit successful operation.
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace sparkplug::reconstruction
{
    struct spNetworkAddressForAnalysis
    {
        std::array<std::uint8_t,16> bytes{};
        std::uint16_t knownBytes=0; // Native constructor leaves all 16 bytes unwritten.
        void Clear() noexcept {bytes.fill(0);knownBytes=0xffff;}
        void Put16(unsigned offset,std::uint16_t value) noexcept
        {for(unsigned i=0;i<2;++i){bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));knownBytes|=static_cast<std::uint16_t>(1u<<(offset+i));}}
        void Put32(unsigned offset,std::uint32_t value) noexcept
        {for(unsigned i=0;i<4;++i){bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));knownBytes|=static_cast<std::uint16_t>(1u<<(offset+i));}}
        [[nodiscard]] std::uint32_t Read(unsigned offset,unsigned count) const
        {
            std::uint32_t value=0;
            for(unsigned i=0;i<count;++i)
            {
                if(!(knownBytes&(1u<<(offset+i))))throw std::logic_error("Unwritten native network address byte");
                value|=std::uint32_t(bytes[offset+i])<<(8*i);
            }
            return value;
        }
    };
    struct spNetworkSocketSetForAnalysis
    {
        std::uint32_t count=0;
        std::array<std::uint32_t,64> sockets{};
    };
    struct spNetworkGlobalsForAnalysis
    {
        std::uint32_t instanceCount=0;
        // Native selectors 0=read, 1=write, 2=exception; shared by all instances.
        std::array<spNetworkSocketSetForAnalysis,3> sets{};
        std::uint32_t failedAcceptWord=0xffffffff; // PC global 007403A0.
    };
    class spDXNetworkHost
    {
    public:
        virtual ~spDXNetworkHost()=default;
        spNetworkGlobalsForAnalysis globals;
        virtual int Startup(std::uint16_t version)=0;
        virtual int Cleanup() noexcept=0;
        virtual std::uint32_t Socket(int family,int type,int protocol)=0;
        virtual std::uint16_t Htons(std::uint16_t value)=0;
        virtual std::uint32_t Htonl(std::uint32_t value)=0;
        virtual std::uint16_t Ntohs(std::uint16_t value)=0;
        virtual std::uint32_t Ntohl(std::uint32_t value)=0;
        virtual int Connect(std::uint32_t socket,const spNetworkAddressForAnalysis& address)=0;
        virtual int Bind(std::uint32_t socket,const spNetworkAddressForAnalysis& address)=0;
        // length enters getsockname with the original bind address argument,
        // not 16. Preserve this original peculiarity in adapters/fixtures.
        virtual int GetSockName(std::uint32_t socket,spNetworkAddressForAnalysis& address,std::uint32_t& length)=0;
        virtual int Listen(std::uint32_t socket,int backlog)=0;
        virtual int CloseSocket(std::uint32_t socket) noexcept=0;
        virtual int IoctlSocket(std::uint32_t socket,std::uint32_t command,std::uint32_t& value)=0;
        virtual int Send(std::uint32_t socket,const void* data,int size,int flags)=0;
        virtual int SendTo(std::uint32_t socket,const void* data,int size,int flags,const spNetworkAddressForAnalysis& address)=0;
        virtual int Recv(std::uint32_t socket,void* data,int size,int flags)=0;
        virtual int RecvFrom(std::uint32_t socket,void* data,int size,int flags,spNetworkAddressForAnalysis& address,std::uint32_t& length)=0;
        virtual std::uint32_t Accept(std::uint32_t socket,spNetworkAddressForAnalysis& address,std::uint32_t& length)=0;
        virtual int LastError()=0;
        virtual int Select(int nfds,spNetworkSocketSetForAnalysis& read,spNetworkSocketSetForAnalysis& write,
                           spNetworkSocketSetForAnalysis& except,std::int32_t seconds,std::int32_t microseconds)=0;
        // Host represents valid C-string/hostent outputs; absent outputs fail
        // explicitly instead of simulating native uninitialized/null reads.
        virtual int GetHostName(std::optional<std::string>& name,int capacity)=0;
        virtual std::uint32_t GetHostByName(const std::string& name)=0;
    };

    // Explicit application setup for parameterless RTTI factories. A direct
    // constructor can instead receive a shared host. Existing objects retain
    // their host when this factory service is replaced or removed.
    void SetDXNetworkFactoryHostForAnalysis(std::shared_ptr<spDXNetworkHost> host);
    [[nodiscard]] std::shared_ptr<spDXNetworkHost> GetDXNetworkFactoryHostForAnalysis();
}
