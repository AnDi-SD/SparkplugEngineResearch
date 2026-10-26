#pragma once
// Inferred header/path and analytical operation names. PC component; host
// storage and explicit ownership do not claim the original36-byte C++ ABI.
#include "../SparkBase/spBaseObject.h"
#include "../SparkBase/spMemoryStream.h"
#include "spNetworkPacket.h"
#include "../../Analysis/Host/spNetworkPeerHost.h"

namespace sparkplug::reconstruction
{
    class spNetworkPeer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x18b4576b;
        struct StateForAnalysis
        {
            std::uint16_t id;
            std::uint8_t byte12,unknown13;
            std::uint32_t timestamp;
        };
        explicit spNetworkPeer(std::shared_ptr<spNetworkPeerHost> host=GetNetworkPeerFactoryHostForAnalysis());
        ~spNetworkPeer() override;
        spNetworkPeer(const spNetworkPeer&)=delete;
        spNetworkPeer& operator=(const spNetworkPeer&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool OpenForAnalysis(std::uint8_t tcpOnly);
        [[nodiscard]] bool CloseForAnalysis();
        // Owns incoming TCP stream; replacement deletes the previous stream
        // without Close. UDP inherits its address and the supplied16-bit port.
        [[nodiscard]] bool AdoptTcpForAnalysis(std::unique_ptr<spSocketStream>,std::uint16_t udpPort);
        [[nodiscard]] int SendPacketForAnalysis(spNetworkPacket&,std::uint8_t usePeerDestination);
        [[nodiscard]] int SendForAnalysis(std::uint16_t packetSource,std::uint16_t type,
            const void* data,std::uint16_t size,std::uint8_t tcp,std::uint8_t useBroadcastDestination,
            std::uint32_t field1,std::uint32_t field2);
        // Explicit producer setup; original setter names are not established.
        void SetIdForAnalysis(std::uint16_t value) noexcept{id_=value;}
        void SetTimestampForAnalysis(std::uint32_t value) noexcept{timestamp_=value;}
        // Server Ready writes native byte+13. This explicit producer setter
        // does not claim an original Peer setter declaration or packet policy.
        void SetReadyByteForAnalysis(std::uint8_t value) noexcept{unknown13_=value;}
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const noexcept{return {id_,byte12_,unknown13_,timestamp_};}
        [[nodiscard]] spSocketStream& TcpForAnalysis() noexcept{return *tcp_;}
        [[nodiscard]] spSocketStream& UdpForAnalysis() noexcept{return *udp_;}
        [[nodiscard]] spMemoryStream& BufferForAnalysis() noexcept{return *buffer_;}
    private:
        void LogFailure(const char*);
        std::shared_ptr<spNetworkPeerHost> host_;
        std::uint16_t id_=0xbad0;
        std::uint8_t byte12_=0,unknown13_=0;
        std::uint32_t timestamp_=0;
        std::unique_ptr<spSocketStream> tcp_,udp_;
        std::unique_ptr<spMemoryStream> buffer_;
    };
}
