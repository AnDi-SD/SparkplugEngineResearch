#include "spNetworkMatchmaking.h"
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkMatchmakingHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNetworkMatchmaking>();}
        const spRTTIRecord Record{spNetworkMatchmaking::ClassID,spBaseObject::ClassID,
            "spNetworkMatchmaking",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    void SetNetworkMatchmakingFactoryHostForAnalysis(std::shared_ptr<spNetworkMatchmakingHost> host){FactoryHost=std::move(host);}
    std::shared_ptr<spNetworkMatchmakingHost> GetNetworkMatchmakingFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkMatchmaking::spNetworkMatchmaking(std::shared_ptr<spNetworkMatchmakingHost> host):host_(std::move(host))
    {
        if(!host_)throw std::invalid_argument("NetworkMatchmaking requires a foreign-service host");
        memoryStream_=std::make_unique<spMemoryStream>();
        socketStream_=host_->CreateSocketStream();
        // Portable protection: native dereferences a null socket on Pump/Close.
        if(!socketStream_)throw std::runtime_error("NetworkMatchmaking socket stream is unavailable");
    }
    spNetworkMatchmaking::~spNetworkMatchmaking()
    {
        // Native releases the memory stream FIRST, without calling own Close.
        memoryStream_.reset();
        if(socketStream_){host_->DeleteSocketStream(socketStream_);socketStream_=0;}
    }
    const spRTTIRecord& spNetworkMatchmaking::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetworkMatchmaking::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkMatchmaking::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spNetworkMatchmaking>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool spNetworkMatchmaking::CloseForAnalysis()
    {
        if(host_->CloseSocketStream(socketStream_))return true;
        if(const auto debug=host_->ResolveNetworkDebug())
            host_->Log(debug,0,"spNetworkMatchmaking","Closing connections with MM server failed",{});
        return false;
    }
    void spNetworkMatchmaking::PumpForAnalysis()
    {
        if(!host_->SocketConnected(socketStream_)||!memoryStream_)return;
        (void)memoryStream_->Open("MM TCP packet buffer");
        if(host_->ReceiveSocketStream(socketStream_,*memoryStream_))
        {
            std::uint32_t size=0;
            (void)memoryStream_->GetSize(&size);
            if(!size)
            {
                if(host_->ResolveNetworkDebug())
                    host_->Log(host_->ResolveZeroReplyDebug(),1,"spNetworkMatchmaking","0 byte received, connection closed",{});
                (void)CloseForAnalysis();
            }
            else
            {
                spNetworkMatchmakingNotificationForAnalysis notification;
                notification.sender=this;
                (void)memoryStream_->GetSize(&notification.size);
                notification.data=memoryStream_->GetBuffer();
                host_->Dispatch(*this,notification);
                // Native uses the FIRST size but repeatedly asks GetBuffer AFTER
                // dispatch. Subscribers may have changed the stream meanwhile.
                std::string dump;
                constexpr char Hex[]="0123456789ABCDEF";
                for(std::uint32_t i=0;i<size;++i)
                {
                    const auto* bytes=static_cast<const std::uint8_t*>(memoryStream_->GetBuffer());
                    if(!bytes||i>=memoryStream_->GetCapacity())
                        throw std::runtime_error("NetworkMatchmaking subscriber invalidated the borrowed buffer");
                    const auto v=bytes[i];dump+=Hex[v>>4];dump+=Hex[v&15];dump+=' ';
                }
                // Native then appends the empty string at006EBBF0.
                if(const auto debug=host_->ResolveNetworkDebug())
                    host_->Log(debug,1,"spNetworkMatchmaking","Packet Content:%s",dump);
            }
        }
        (void)memoryStream_->Close(); // Also after failed Receive; result ignored.
    }
    spNetworkMatchmaking::StateForAnalysis spNetworkMatchmaking::GetStateForAnalysis() const
    {return {socketStream_!=0,bool(memoryStream_),text_};}
}
