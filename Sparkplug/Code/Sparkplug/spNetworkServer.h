#pragma once
// Inferred declarations and analytical names. Portable ownership/storage is
// independent of the shipped 32-bit PC object; no PS2 counterpart is inferred.
#include "../SparkBase/spBaseObject.h"
#include "../SparkBase/spMemoryStream.h"
#include "spNetworkPeer.h"
#include "../../Analysis/Host/spNetworkServerHost.h"
#include <list>
#include <optional>

namespace sparkplug::reconstruction
{
    class spNetworkServer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x7259443e;
        using Handle=spNetworkServerHost::Handle;
        struct StateForAnalysis
        {
            std::uint16_t id;
            std::optional<std::uint16_t> maximumId;
            std::uint32_t acceptedCount;
            std::vector<std::uint16_t> peerIds,availableIds;
            bool hasQueue;
        };
        struct PeerStateForAnalysis
        {std::uint16_t id;std::uint8_t ready;std::uint32_t timestamp;bool connected;};
        explicit spNetworkServer(std::shared_ptr<spNetworkServerHost> host=GetNetworkServerFactoryHostForAnalysis());
        ~spNetworkServer() override;
        spNetworkServer(const spNetworkServer&)=delete;
        spNetworkServer& operator=(const spNetworkServer&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool OpenForAnalysis();
        [[nodiscard]] bool CloseForAnalysis();
        [[nodiscard]] bool SetQueueForAnalysis(Handle);
        [[nodiscard]] bool SetPeerForAnalysis(spNetworkPeer&);
        // Explicit portable transfer for native accepted peers, not a recovered
        // TCP accept/spawn routine. byte12 must identify an accepted peer.
        void AdoptPeerForAnalysis(std::unique_ptr<spNetworkPeer>);
        [[nodiscard]] bool FindPeerForAnalysis(spNetworkPeer*&,std::uint16_t);
        [[nodiscard]] std::uint32_t CountOwnedPeersForAnalysis();
        void PruneDisconnectedForAnalysis();
        [[nodiscard]] bool RemovePeerForAnalysis(spNetworkPeer&);
        [[nodiscard]] bool BroadcastForAnalysis(spNetworkPacket&);
        void ReleaseIdForAnalysis(std::uint16_t);
        [[nodiscard]] std::uint16_t AllocateIdForAnalysis();
        [[nodiscard]] bool HandleInitialRequestForAnalysis(spNetworkPeer&,const spNetworkPacket&);
        [[nodiscard]] bool HandleInitialReplyForAnalysis(const spNetworkPacket&);
        [[nodiscard]] bool HandleReadyForAnalysis(spNetworkPeer*,const spNetworkPacket&);
        [[nodiscard]] bool HandleHelloForAnalysis(spNetworkPeer*,spNetworkPacket&);
        [[nodiscard]] bool ProcessPacketsForAnalysis(spNetworkPeer*,spStream&);
        // True consumes incoming. Capacity refusal leaves it with the caller.
        // Adoption failure consumes it into the retained orphan Peer and gives
        // false; the original worker would then create a dangling peer link.
        [[nodiscard]] bool HandleNewTcpConnectionForAnalysis(std::unique_ptr<spSocketStream>& incoming);
        void RunWorkerForAnalysis();
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const;
        // Observation requires external synchronization, as does State above.
        [[nodiscard]] std::vector<PeerStateForAnalysis> GetPeerStatesForAnalysis() const;
        // Literal field producer setup. +16 is not initialized by constructor.
        void SetStateForAnalysis(std::uint16_t id,std::uint16_t maximumId,std::uint32_t acceptedCount) noexcept;
        void SetIdForAnalysis(std::uint16_t id) noexcept{id_=id;}
        [[nodiscard]] spSocketStream& TcpForAnalysis() noexcept{return *tcp_;}
        [[nodiscard]] spSocketStream& UdpForAnalysis() noexcept{return *udp_;}
        [[nodiscard]] spMemoryStream& BufferForAnalysis() noexcept{return *buffer_;}
    private:
        struct Peer {spNetworkPeer* object;std::unique_ptr<spNetworkPeer> owner;};
        void Log(std::int32_t,const char*,const std::vector<std::uint32_t>& arguments={});
        std::uint32_t TimerMilliseconds();
        std::shared_ptr<spNetworkServerHost> host_;
        Handle lock_=0,thread_=0,queue_=0;
        std::uint16_t id_=0xbad0;
        std::optional<std::uint16_t> maximumId_;
        std::uint32_t acceptedCount_=0;
        std::list<std::uint16_t> availableIds_;
        std::list<Peer> peers_;
        std::unique_ptr<spSocketStream> tcp_,udp_;
        std::unique_ptr<spMemoryStream> buffer_;
    };
}
