#pragma once
// Our foreign socket/global/notification boundary, not an original backend.
#include "../../Code/SparkBase/spMemoryStream.h"
#include <cstdint>
#include <memory>
#include <string>

namespace sparkplug::reconstruction
{
    class spNetworkMatchmaking;
    struct spNetworkMatchmakingNotificationForAnalysis
    {
        std::uint32_t code=0x22,word04=0,word08=0,word0C=0;
        const spNetworkMatchmaking* sender=nullptr;
        std::uint32_t word14=0;
        const void* data=nullptr; // Native word18, borrowed during dispatch.
        std::uint32_t size=0;    // Native word1C, second GetSize result.
    };
    class spNetworkMatchmakingHost
    {
    public:
        using Handle=std::uintptr_t;
        virtual ~spNetworkMatchmakingHost()=default;
        virtual Handle CreateSocketStream()=0;
        virtual void DeleteSocketStream(Handle) noexcept=0;
        virtual std::uint8_t SocketConnected(Handle)=0;
        virtual bool CloseSocketStream(Handle)=0;
        virtual bool ReceiveSocketStream(Handle,spMemoryStream&)=0;
        // Resolve current NetworkManager (including its lazy factory) and +70.
        virtual Handle ResolveNetworkDebug()=0;
        // Native zero-size branch re-resolves the same current manager through
        // helper0041B520 and reads +70 AGAIN, without a second null check.
        virtual Handle ResolveZeroReplyDebug()=0;
        virtual void Log(Handle,std::int32_t level,const std::string& category,
            const std::string& format,const std::string& argument)=0;
        virtual void Dispatch(spNetworkMatchmaking&,const spNetworkMatchmakingNotificationForAnalysis&)=0;
    };
    void SetNetworkMatchmakingFactoryHostForAnalysis(std::shared_ptr<spNetworkMatchmakingHost>);
    [[nodiscard]] std::shared_ptr<spNetworkMatchmakingHost> GetNetworkMatchmakingFactoryHostForAnalysis();
}
