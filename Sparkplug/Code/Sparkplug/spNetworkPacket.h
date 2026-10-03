#pragma once
// PC behavior is executable-backed. Translation-unit/header paths, method
// names, namespace, safe storage and the unknown-value representation are ours.
#include "../SparkBase/spStream.h"
#include "../../Analysis/Host/spNetworkPacketHost.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction
{
    struct spNetworkPacketHeaderForAnalysis
    {
        std::uint16_t source=0xbad0,destination=0xbad0;
        // Native constructor leaves these two members unwritten.
        std::optional<std::uint16_t> packetType;
        std::optional<std::uint8_t> useTcp;
        std::uint32_t dataField1=0,dataField2=0;
        std::uint16_t size=0;
    };
    class spNetworkPacket final:public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x0546DE34;
        static constexpr std::size_t PayloadCapacity=256;
        explicit spNetworkPacket(std::shared_ptr<spNetworkPacketHost> host=GetNetworkPacketFactoryHostForAnalysis());
        ~spNetworkPacket() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool ReadForAnalysis(spStream&);
        [[nodiscard]] bool WriteForAnalysis(spStream&) const;
        [[nodiscard]] const spNetworkPacketHeaderForAnalysis& HeaderForAnalysis() const noexcept{return header_;}
        void SetHeaderForAnalysis(spNetworkPacketHeaderForAnalysis header) noexcept{header_=header;}
        // Explicit external population, not an inferred original method.
        void SetPayloadForAnalysis(const void*,std::size_t);
        [[nodiscard]] const std::uint8_t* KnownPayloadForAnalysis(std::size_t) const;
        [[nodiscard]] std::size_t KnownPayloadPrefixForAnalysis() const noexcept{return knownPrefix_;}
    private:
        [[nodiscard]] bool ReadFailure(const char*) const;
        [[nodiscard]] bool WriteFailure(spStream&,const char*) const;
        std::shared_ptr<spNetworkPacketHost> host_;
        spNetworkPacketHeaderForAnalysis header_;
        std::unique_ptr<std::array<std::uint8_t,PayloadCapacity>> payload_;
        std::size_t knownPrefix_=0;
    };
}
