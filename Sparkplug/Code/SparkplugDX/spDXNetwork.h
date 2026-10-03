#pragma once
// Recovered PC behavior; original method/header names remain unknown.
// 17 primary slots 006EF928; secondary object table 006EF90C; native size 40h.
#include "../Sparkplug/spNetwork.h"
#include "../../Analysis/Host/spDXNetworkHost.h"

namespace sparkplug::reconstruction
{
    class spDXNetwork final : public spNetwork
    {
    public:
        static constexpr spClassID ClassID=0x41FB6C73;
        static constexpr std::uint32_t InvalidSocket=0xffffffff;
        spDXNetwork();
        explicit spDXNetwork(std::shared_ptr<spDXNetworkHost> host);
        ~spDXNetwork() override;
        spDXNetwork(const spDXNetwork&)=delete;
        spDXNetwork& operator=(const spDXNetwork&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool OpenForAnalysis(std::uint32_t type) override;
        bool ConnectForAnalysis(std::uint32_t address,std::uint16_t port) override;
        bool BindForAnalysis(std::uint32_t address,std::uint16_t port) override;
        bool ListenForAnalysis() override;
        spNetworkAcceptForAnalysis AcceptForAnalysis() override;
        bool CloseForAnalysis() override;
        bool SetBlockingForAnalysis(std::uint8_t blocking) override;
        bool AddToSetForAnalysis(std::uint32_t selector) override;
        bool RemoveFromSetForAnalysis(std::uint32_t selector) override;
        bool ClearSetForAnalysis(std::uint32_t selector) override;
        bool SelectForAnalysis(std::uint32_t timeout) override;
        int SendForAnalysis(const void* data,int size) override;
        int SendToForAnalysis(std::uint32_t address,std::uint16_t port,const void* data,int size) override;
        int ReceiveForAnalysis(void* data,int size) override;
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void* data,int size) override;
        bool GetLocalAddressForAnalysis(std::uint32_t& address) override;
        const char* GetLocalAddressTextForAnalysis() override;
        [[nodiscard]] std::uint32_t GetSocketForAnalysis() const noexcept{return socket_;}
        [[nodiscard]] const spNetworkAddressForAnalysis& GetSockAddressForAnalysis() const noexcept{return socketAddress_;}
    private:
        void MakeAddress(std::uint32_t,std::uint16_t);
        std::shared_ptr<spDXNetworkHost> host_;
        std::uint32_t socket_=InvalidSocket;
        spNetworkAddressForAnalysis socketAddress_;
        std::optional<std::string> localAddressText_;
    };
}
