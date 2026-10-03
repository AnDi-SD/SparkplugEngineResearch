// Inferred translation-unit path; no spNetworkPacket.cpp path survives in PC.
#include "spNetworkPacket.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkPacketHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNetworkPacket>();}
        const spRTTIRecord Record{spNetworkPacket::ClassID,spBaseObject::ClassID,
            "spNetworkPacket",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        template<class T> bool ReadLittle(spStream& stream,T& value)
        {
            std::array<std::uint8_t,sizeof(T)> bytes;
            // Seed with the old value so a failing, non-mutating stream does
            // not alter the packet. Qualified native MemoryStream failures
            // do not write destination bytes.
            for(unsigned i=0;i<sizeof(T);++i)bytes[i]=static_cast<std::uint8_t>(value>>(i*8));
            const bool ok=stream.ReadData(bytes.data(),static_cast<std::uint32_t>(bytes.size()));
            value=0;for(unsigned i=0;i<sizeof(T);++i)value|=T(bytes[i])<<(i*8);
            return ok;
        }
        template<class T> bool WriteLittle(spStream& stream,T value)
        {
            std::array<std::uint8_t,sizeof(T)> bytes;
            for(unsigned i=0;i<sizeof(T);++i)bytes[i]=static_cast<std::uint8_t>(value>>(i*8));
            return stream.WriteData(bytes.data(),static_cast<std::uint32_t>(bytes.size()));
        }
    }
    void SetNetworkPacketFactoryHostForAnalysis(std::shared_ptr<spNetworkPacketHost> h){FactoryHost=std::move(h);}
    std::shared_ptr<spNetworkPacketHost> GetNetworkPacketFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkPacket::spNetworkPacket(std::shared_ptr<spNetworkPacketHost> host):host_(std::move(host)),
        payload_(std::make_unique<std::array<std::uint8_t,PayloadCapacity>>()){}
    spNetworkPacket::~spNetworkPacket(){payload_.reset();} // Native payload is freed before base.
    const spRTTIRecord& spNetworkPacket::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetworkPacket::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkPacket::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spNetworkPacket>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool spNetworkPacket::ReadFailure(const char* text) const
    {
        if(!host_)throw std::logic_error("NetworkPacket diagnostic host is unavailable");
        host_->ReportFailure(text);return false;
    }
    bool spNetworkPacket::WriteFailure(spStream& stream,const char* text) const
    {
        (void)ReadFailure(text);(void)stream.Close();return false;
    }
    bool spNetworkPacket::ReadForAnalysis(spStream& stream)
    {
        std::uint32_t size=0,position=0;
        (void)stream.GetSize(&size);(void)stream.GetCurrentPosition(position);
        if(position>=size)return false;
        if(!ReadLittle(stream,header_.source))return ReadFailure("Error while reading <Source> of received packet.");
        if(!ReadLittle(stream,header_.destination))return ReadFailure("Error while reading <Destination> of received packet.");
        auto type=header_.packetType.value_or(0);
        if(!ReadLittle(stream,type))return ReadFailure("Error while reading <PacketType> of received packet.");
        header_.packetType=type;
        auto tcp=header_.useTcp.value_or(0);
        if(!ReadLittle(stream,tcp))return ReadFailure("Error while reading <UseTcp> of received packet.");
        header_.useTcp=tcp;
        if(!ReadLittle(stream,header_.dataField1))return ReadFailure("Error while reading <PacketDataField1> of received packet.");
        if(!ReadLittle(stream,header_.dataField2))return ReadFailure("Error while reading <PacketDataField2> of received packet.");
        if(!ReadLittle(stream,header_.size))return ReadFailure("Error while reading <Size> of received packet.");
        // Explicit portable guard. Original has a256-byte allocation but no
        // bound check here. Oversize prefixes are preserved, transfer rejected.
        if(header_.size>PayloadCapacity)throw std::length_error("NetworkPacket payload exceeds original allocation");
        if(!stream.ReadData(payload_->data(),header_.size))return ReadFailure("Error while reading <Data> of received packet.");
        knownPrefix_=std::max<std::size_t>(knownPrefix_,header_.size);return true;
    }
    bool spNetworkPacket::WriteForAnalysis(spStream& stream) const
    {
        if(!WriteLittle(stream,header_.source))return WriteFailure(stream,"Error while writing <Source>.");
        if(!WriteLittle(stream,header_.destination))return WriteFailure(stream,"Error while writing <Destination>.");
        if(!header_.packetType)throw std::logic_error("NetworkPacket type is unwritten in the original");
        if(!WriteLittle(stream,*header_.packetType))return WriteFailure(stream,"Error while writing <PacketType>.");
        if(!header_.useTcp)throw std::logic_error("NetworkPacket TCP byte is unwritten in the original");
        if(!WriteLittle(stream,*header_.useTcp))return WriteFailure(stream,"Error while writing <UseTcp>.");
        if(!WriteLittle(stream,header_.dataField1))return WriteFailure(stream,"Error while writing <PacketDataField>.");
        if(!WriteLittle(stream,header_.dataField2))return WriteFailure(stream,"Error while writing <PacketDataField>.");
        if(!WriteLittle(stream,header_.size))return WriteFailure(stream,"Error while writing <Size>.");
        const auto* data=KnownPayloadForAnalysis(header_.size);
        if(!stream.WriteData(data,header_.size))return WriteFailure(stream,"Error while writing <Data>.");
        return true;
    }
    void spNetworkPacket::SetPayloadForAnalysis(const void* data,std::size_t size)
    {
        if(size>PayloadCapacity)throw std::length_error("NetworkPacket payload exceeds original allocation");
        if(size&&!data)throw std::invalid_argument("Null packet payload");
        if(size)std::memcpy(payload_->data(),data,size);knownPrefix_=std::max(knownPrefix_,size);
    }
    const std::uint8_t* spNetworkPacket::KnownPayloadForAnalysis(std::size_t size) const
    {
        if(size>PayloadCapacity)throw std::length_error("NetworkPacket payload exceeds original allocation");
        if(size>knownPrefix_)throw std::logic_error("NetworkPacket payload contains unwritten original bytes");
        return payload_->data();
    }
}
