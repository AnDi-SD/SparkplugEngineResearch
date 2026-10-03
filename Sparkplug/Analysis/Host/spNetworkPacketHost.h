#pragma once
// Our boundary for the original foreign diagnostic helper004135E0.
#include <memory>
namespace sparkplug::reconstruction
{
    class spNetworkPacketHost
    {
    public:
        virtual ~spNetworkPacketHost()=default;
        virtual void ReportFailure(const char* originalMessage)=0;
    };
    void SetNetworkPacketFactoryHostForAnalysis(std::shared_ptr<spNetworkPacketHost>);
    [[nodiscard]] std::shared_ptr<spNetworkPacketHost> GetNetworkPacketFactoryHostForAnalysis();
}
