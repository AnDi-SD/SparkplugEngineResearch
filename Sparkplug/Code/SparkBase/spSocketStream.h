#pragma once
// Implementation path is present in the PC EXE as SparkBase/spSocketStream.cpp.
// Header, declarations and method names below are inferred. Portable ownership
// and layout are not the native 32-bit ABI. No PS2 counterpart is inferred.
#include "spStream.h"
#include "../../Analysis/Host/spSocketStreamHost.h"

namespace sparkplug::reconstruction
{
    class spSocketStream final : public spStream
    {
    public:
        static constexpr spClassID ClassID=0x1ED8677D;
        explicit spSocketStream(std::shared_ptr<spSocketStreamHost>);
        spSocketStream():spSocketStream(GetSocketStreamFactoryHostForAnalysis()){}
        ~spSocketStream() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool Open(const char*) override;
        bool Open(std::uint32_t mode,const char*) override;
        bool Close() override;
        bool Seek(SeekSource,std::int32_t) override;
        bool GetCurrentPosition(std::uint32_t&) const override;
        bool ReadData(void*,std::uint32_t) override;
        bool WriteData(const void*,std::uint32_t) override;
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override;
        bool GetSize(std::uint32_t*) const override;
        // Own nonvirtual helpers: 499340/4993D0/4994E0/499580.
        [[nodiscard]] bool ReceiveStreamForAnalysis(spStream&);
        // GetSize's status is ignored in the original. If it leaves the output
        // unwritten, native uses unspecified stack data. This explicit seed is
        // our analytical storage choice, not an original default value.
        [[nodiscard]] int SendStreamForAnalysis(spStream&,std::uint32_t sizeSeed=0);
        [[nodiscard]] bool AdoptNetworkForAnalysis(spNetworkAcceptForAnalysis);
        [[nodiscard]] std::unique_ptr<spSocketStream> AcceptForAnalysis();
        void SetEndpointForAnalysis(std::uint32_t type,std::uint32_t address,
            std::uint16_t port,std::uint8_t blocking) noexcept;
        [[nodiscard]] spNetwork& NetworkForAnalysis() noexcept{return *network_;}
        [[nodiscard]] std::uint32_t TypeForAnalysis() const noexcept{return type_;}
        [[nodiscard]] std::uint32_t AddressForAnalysis() const noexcept{return address_;}
        [[nodiscard]] std::uint32_t PortWordForAnalysis() const noexcept{return portWord_;}
        [[nodiscard]] std::uint8_t BlockingForAnalysis() const noexcept{return blocking_;}
    private:
        std::shared_ptr<spSocketStreamHost> host_;
        std::unique_ptr<spNetwork> network_;
        std::uint32_t type_=1,address_=0xffffffff,portWord_=0xffff;
        std::uint8_t blocking_=0;
    };
}
