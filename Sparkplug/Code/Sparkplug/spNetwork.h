#pragma once
// Inferred header/path and declarations. PC has a primary network interface
// and a secondary CrossPlatform object interface at +4. Host C++ layout is not
// that 32-bit ABI. Registration alone was not used to infer these two bases.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spNetwork;
    struct spNetworkAcceptForAnalysis
    {
        std::unique_ptr<spNetwork> object;
        std::uint32_t failureWord=0; // Native returns 0 for WSAEWOULDBLOCK, global sentinel otherwise.
    };
    class spNetworkInterfaceForAnalysis
    {
    public:
        virtual ~spNetworkInterfaceForAnalysis()=default;
        virtual bool OpenForAnalysis(std::uint32_t type)=0;
        virtual bool ConnectForAnalysis(std::uint32_t address,std::uint16_t port)=0;
        virtual bool BindForAnalysis(std::uint32_t address,std::uint16_t port)=0;
        virtual bool ListenForAnalysis()=0;
        virtual spNetworkAcceptForAnalysis AcceptForAnalysis()=0;
        virtual bool CloseForAnalysis()=0;
        virtual bool SetBlockingForAnalysis(std::uint8_t blocking)=0;
        virtual bool AddToSetForAnalysis(std::uint32_t selector)=0;
        virtual bool RemoveFromSetForAnalysis(std::uint32_t selector)=0;
        virtual bool ClearSetForAnalysis(std::uint32_t selector)=0;
        virtual bool SelectForAnalysis(std::uint32_t timeout)=0;
        virtual int SendForAnalysis(const void* data,int size)=0;
        virtual int SendToForAnalysis(std::uint32_t address,std::uint16_t port,const void* data,int size)=0;
        virtual int ReceiveForAnalysis(void* data,int size)=0;
        // First two original words are ignored; their source types are unknown.
        virtual int ReceiveFromForAnalysis(std::uint32_t unknown1,std::uint32_t unknown2,void* data,int size)=0;
        virtual bool GetLocalAddressForAnalysis(std::uint32_t& address)=0;
        virtual const char* GetLocalAddressTextForAnalysis()=0;
    };
    class spNetwork : public spNetworkInterfaceForAnalysis, public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID=0x18B4576A;
        ~spNetwork() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        // Base primary+40 also points to the original null-return stub.
        const char* GetLocalAddressTextForAnalysis() override;
        [[nodiscard]] std::uint8_t IsConnectedForAnalysis() const noexcept{return connected_;}
        [[nodiscard]] std::uint32_t GetTypeForAnalysis() const noexcept{return type_;}
        [[nodiscard]] std::uint32_t GetAddressForAnalysis() const noexcept{return address_;}
        [[nodiscard]] std::uint16_t GetPortForAnalysis() const noexcept{return port_;}
    protected:
        // Original protected constructor004A1C40, independently executed.
        // Access level is a portable choice; the native base has no factory.
        spNetwork() noexcept;
        std::uint8_t connected_;
        std::uint32_t type_,address_;
        std::uint16_t port_;
    };
}
