#pragma once
// Our foreign-service boundary. SocketStream/Packet/MemoryStream operations
// execute in the shared recovered classes; this is not an original backend.
#include "../../Code/SparkBase/spSocketStream.h"
#include <cstdint>
#include <memory>
#include <string>

namespace sparkplug::reconstruction
{
    class spNetworkPeerHost
    {
    public:
        using Handle=std::uintptr_t;
        virtual ~spNetworkPeerHost()=default;
        virtual std::unique_ptr<spSocketStream> CreateSocketStream()=0;
        // Resolve the current manager (including its lazy factory) and +70.
        virtual Handle ResolveNetworkDebug()=0;
        virtual void Log(Handle,std::int32_t,const std::string& category,const std::string& text)=0;
    };
    void SetNetworkPeerFactoryHostForAnalysis(std::shared_ptr<spNetworkPeerHost>);
    [[nodiscard]] std::shared_ptr<spNetworkPeerHost> GetNetworkPeerFactoryHostForAnalysis();
}
