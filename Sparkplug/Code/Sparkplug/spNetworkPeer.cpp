#include "spNetworkPeer.h"
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkPeerHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNetworkPeer>();}
        const spRTTIRecord Record{spNetworkPeer::ClassID,spBaseObject::ClassID,
            "spNetworkPeer",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    void SetNetworkPeerFactoryHostForAnalysis(std::shared_ptr<spNetworkPeerHost> h){FactoryHost=std::move(h);}
    std::shared_ptr<spNetworkPeerHost> GetNetworkPeerFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkPeer::spNetworkPeer(std::shared_ptr<spNetworkPeerHost> h):host_(std::move(h))
    {
        if(!host_)throw std::invalid_argument("NetworkPeer requires a foreign-service host");
        tcp_=host_->CreateSocketStream();
        if(!tcp_)throw std::runtime_error("NetworkPeer TCP stream is unavailable");
        tcp_->SetEndpointForAnalysis(1,tcp_->AddressForAnalysis(),static_cast<std::uint16_t>(tcp_->PortWordForAnalysis()),tcp_->BlockingForAnalysis());
        udp_=host_->CreateSocketStream();
        if(!udp_)throw std::runtime_error("NetworkPeer UDP stream is unavailable");
        udp_->SetEndpointForAnalysis(0,udp_->AddressForAnalysis(),static_cast<std::uint16_t>(udp_->PortWordForAnalysis()),udp_->BlockingForAnalysis());
        buffer_=std::make_unique<spMemoryStream>();
        (void)buffer_->Open("packet buffer");
    }
    spNetworkPeer::~spNetworkPeer()
    {
        (void)tcp_->Close();(void)udp_->Close();
        tcp_.reset();udp_.reset();
        (void)buffer_->Close();buffer_.reset();
    }
    const spRTTIRecord& spNetworkPeer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetworkPeer::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkPeer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spNetworkPeer>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    void spNetworkPeer::LogFailure(const char* text)
    {if(const auto debug=host_->ResolveNetworkDebug())host_->Log(debug,0,"Network Peer",text);}
    bool spNetworkPeer::OpenForAnalysis(std::uint8_t tcpOnly)
    {
        bool result=tcp_->Open(2,nullptr);
        if(result&&!tcpOnly)result=udp_->Open(2,nullptr);
        if(!result)LogFailure("Opening connections failed");
        return result;
    }
    bool spNetworkPeer::CloseForAnalysis()
    {(void)tcp_->Close();(void)udp_->Close();return true;}
    bool spNetworkPeer::AdoptTcpForAnalysis(std::unique_ptr<spSocketStream> incoming,std::uint16_t port)
    {
        if(!incoming)throw std::invalid_argument("NetworkPeer adoption requires a TCP stream");
        tcp_.reset();tcp_=std::move(incoming);byte12_=1;
        udp_->SetEndpointForAnalysis(udp_->TypeForAnalysis(),tcp_->AddressForAnalysis(),port,udp_->BlockingForAnalysis());
        return udp_->Open(2,nullptr);
    }
    int spNetworkPeer::SendPacketForAnalysis(spNetworkPacket& packet,std::uint8_t usePeerDestination)
    {
        if(!tcp_->NetworkForAnalysis().IsConnectedForAnalysis())
        {LogFailure("Failed: no connection");return -1;}
        if(usePeerDestination)
        {auto header=packet.HeaderForAnalysis();header.destination=id_;packet.SetHeaderForAnalysis(header);}
        (void)buffer_->Reset();
        (void)packet.WriteForAnalysis(*buffer_);
        std::uint32_t ignored=0;(void)buffer_->GetSize(&ignored);
        if(!packet.HeaderForAnalysis().useTcp)throw std::logic_error("NetworkPeer packet TCP byte is unwritten");
        auto& stream=*packet.HeaderForAnalysis().useTcp?*tcp_:*udp_;
        return stream.SendStreamForAnalysis(*buffer_);
    }
    int spNetworkPeer::SendForAnalysis(std::uint16_t packetSource,std::uint16_t type,
        const void* data,std::uint16_t size,std::uint8_t tcp,std::uint8_t useBroadcastDestination,
        std::uint32_t field1,std::uint32_t field2)
    {
        // Native ignores Fill's false result and can then read beyond the
        // original payload allocation. Keep our existing explicit safe domain.
        if(size>spNetworkPacket::PayloadCapacity)
            throw std::length_error("NetworkPacket payload exceeds original allocation");
        spNetworkPacket packet;
        (void)packet.FillForAnalysis(packetSource,useBroadcastDestination?0xffff:id_,
            type,tcp,field1,field2,size,data);
        return SendPacketForAnalysis(packet,0);
    }
}
