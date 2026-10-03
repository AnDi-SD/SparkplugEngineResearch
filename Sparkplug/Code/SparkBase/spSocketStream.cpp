// Original PC path: Z:\Sparkplug\Code\SparkBase\spSocketStream.cpp.
#include "spSocketStream.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spSocketStreamHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spSocketStream>();}
        const spRTTIRecord Record{spSocketStream::ClassID,spStream::ClassID,
            "spSocketStream",&spStream::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        int SignedSize(std::uint32_t value) noexcept
        {
            static_assert(sizeof(int)==sizeof(value));
            int result;std::memcpy(&result,&value,sizeof(value));return result;
        }
    }
    void SetSocketStreamFactoryHostForAnalysis(std::shared_ptr<spSocketStreamHost> host){FactoryHost=std::move(host);}
    std::shared_ptr<spSocketStreamHost> GetSocketStreamFactoryHostForAnalysis(){return FactoryHost;}
    spSocketStream::spSocketStream(std::shared_ptr<spSocketStreamHost> host):host_(std::move(host))
    {
        if(!host_)throw std::invalid_argument("SocketStream requires a network host");
        network_=host_->CreateNetwork();
        if(!network_)throw std::runtime_error("SocketStream network factory returned null");
    }
    spSocketStream::~spSocketStream(){network_.reset();} // No own Close call.
    const spRTTIRecord& spSocketStream::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spSocketStream::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spSocketStream::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spSocketStream>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool spSocketStream::Open(const char*){return false;}
    bool spSocketStream::Open(std::uint32_t mode,const char*)
    {
        if(!network_->OpenForAnalysis(type_))return false;
        if(mode==1)
        {
            if(!network_->BindForAnalysis(address_,static_cast<std::uint16_t>(portWord_)))return false;
            if(type_==1&&!network_->ListenForAnalysis())return false;
        }
        else if(mode==2)
        {
            if(!network_->ConnectForAnalysis(address_,static_cast<std::uint16_t>(portWord_)))return false;
        }
        else return false;
        return network_->SetBlockingForAnalysis(blocking_);
    }
    bool spSocketStream::Close(){return network_->CloseForAnalysis();}
    bool spSocketStream::Seek(SeekSource,std::int32_t){return true;}
    bool spSocketStream::GetCurrentPosition(std::uint32_t&) const{return true;}
    bool spSocketStream::ReadData(void*,std::uint32_t){return false;}
    bool spSocketStream::WriteData(const void* data,std::uint32_t size)
    {return network_->SendForAnalysis(data,SignedSize(size))!=-1;}
    bool spSocketStream::vfunc_WriteFromStream(spStream*,std::uint32_t){return false;}
    bool spSocketStream::GetSize(std::uint32_t*) const{return true;}
    bool spSocketStream::ReceiveStreamForAnalysis(spStream& destination)
    {
        // Native reserves 2000 stack bytes but requests only 500 per receive.
        std::array<std::uint8_t,500> buffer;
        int received;
        do
        {
            received=network_->ReceiveForAnalysis(buffer.data(),500);
            if(received!=-1)
            {
                // Explicit portable protection for invalid foreign results;
                // the native program would read beyond its supplied data.
                if(received<0||received>500)throw std::runtime_error("Socket receive result exceeds the qualified buffer");
                if(!destination.WriteData(buffer.data(),static_cast<std::uint32_t>(received)))return false;
            }
        }while(received==500);
        return received!=-1;
    }
    int spSocketStream::SendStreamForAnalysis(spStream& source,std::uint32_t size)
    {
        (void)source.GetSize(&size);
        // Native captures the network dispatch before GetBuffer but reloads
        // its receiver afterwards. Network replacement during GetBuffer is
        // outside this portable lifetime contract.
        return network_->SendForAnalysis(source.GetBuffer(),SignedSize(size));
    }
    bool spSocketStream::AdoptNetworkForAnalysis(spNetworkAcceptForAnalysis input)
    {
        if(!input.object)
        {
            if(!input.failureWord)throw std::invalid_argument("Native adoption dereferences a null network");
            host_->ReportInvalidNetwork();return false;
        }
        network_.reset();
        network_=std::move(input.object);
        type_=network_->GetTypeForAnalysis();address_=network_->GetAddressForAnalysis();
        portWord_=network_->GetPortForAnalysis(); // Zero-extended native 16-bit port.
        return true; // Blocking byte is preserved.
    }
    std::unique_ptr<spSocketStream> spSocketStream::AcceptForAnalysis()
    {
        auto incoming=network_->AcceptForAnalysis();
        if(!incoming.object&&!incoming.failureWord)return nullptr;
        auto stream=std::make_unique<spSocketStream>();
        (void)stream->AdoptNetworkForAnalysis(std::move(incoming)); // Native ignores false.
        return stream;
    }
    void spSocketStream::SetEndpointForAnalysis(std::uint32_t type,std::uint32_t address,
        std::uint16_t port,std::uint8_t blocking) noexcept
    {type_=type;address_=address;portWord_=port;blocking_=blocking;}
}
