#include "spDXNetwork.h"
#include <algorithm>
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spDXNetworkHost> FactoryHost;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spDXNetwork>();}
        const spRTTIRecord Record{spDXNetwork::ClassID,spNetwork::ClassID,"spDXNetwork",&spNetwork::StaticRTTI(),Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        std::int32_t SignedWord(std::uint32_t word) noexcept
        {std::int32_t value;std::memcpy(&value,&word,sizeof(value));return value;}
    }
    void SetDXNetworkFactoryHostForAnalysis(std::shared_ptr<spDXNetworkHost> host){FactoryHost=std::move(host);}
    std::shared_ptr<spDXNetworkHost> GetDXNetworkFactoryHostForAnalysis(){return FactoryHost;}
    const spRTTIRecord& spDXNetwork::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXNetwork::vfunc_18() const noexcept{return StaticRTTI();}
    spDXNetwork::spDXNetwork():spDXNetwork(GetDXNetworkFactoryHostForAnalysis()){}
    spDXNetwork::spDXNetwork(std::shared_ptr<spDXNetworkHost> host):host_(std::move(host))
    {
        if(!host_)throw std::logic_error("spDXNetwork requires an explicit platform host");
        if(host_->globals.instanceCount==0)(void)host_->Startup(0x202);
        ++host_->globals.instanceCount; // Native ignores the WSAStartup result.
    }
    spDXNetwork::~spDXNetwork()
    {
        if(--host_->globals.instanceCount==0)(void)host_->Cleanup();
        // Native teardown only closes connected sockets. An opened/bound
        // unconnected socket must be closed explicitly by the caller.
        if(connected_&&socket_!=InvalidSocket)
        {(void)host_->CloseSocket(socket_);socket_=InvalidSocket;connected_=0;}
    }
    std::unique_ptr<spBaseObject> spDXNetwork::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXNetwork>(host_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spDXNetwork::OpenForAnalysis(std::uint32_t type)
    {
        if(socket_!=InvalidSocket)return true;
        if(type>1)return false;
        type_=type;
        socket_=host_->Socket(2,type==0?2:1,type==0?17:6);
        return socket_!=InvalidSocket;
    }
    void spDXNetwork::MakeAddress(std::uint32_t address,std::uint16_t port)
    {
        socketAddress_.Clear();
        socketAddress_.Put32(4,host_->Htonl(address));
        socketAddress_.Put16(2,host_->Htons(port));
        socketAddress_.Put16(0,2);
    }
    bool spDXNetwork::ConnectForAnalysis(std::uint32_t address,std::uint16_t port)
    {
        if(socket_==InvalidSocket)return false;
        MakeAddress(address,port);
        if(host_->Connect(socket_,socketAddress_)==-1)return false;
        address_=address;port_=port;connected_=1;return true;
    }
    bool spDXNetwork::BindForAnalysis(std::uint32_t address,std::uint16_t port)
    {
        if(socket_==InvalidSocket)return false;
        MakeAddress(address==0xffffffff?0:address,port);
        if(host_->Bind(socket_,socketAddress_)==-1)return false;
        socketAddress_.Clear();
        (void)host_->GetSockName(socket_,socketAddress_,address);
        address_=host_->Ntohl(socketAddress_.Read(4,4));
        port_=host_->Ntohs(static_cast<std::uint16_t>(socketAddress_.Read(2,2)));
        return true; // Does not set connected18, and ignores getsockname failure.
    }
    bool spDXNetwork::ListenForAnalysis()
    {return socket_!=InvalidSocket&&host_->Listen(socket_,5)!=-1;}
    bool spDXNetwork::CloseForAnalysis()
    {
        if(socket_!=InvalidSocket)
        {(void)host_->CloseSocket(socket_);socket_=InvalidSocket;connected_=0;}
        return true;
    }
    bool spDXNetwork::SetBlockingForAnalysis(std::uint8_t blocking)
    {
        if(socket_==InvalidSocket)return false;
        std::uint32_t value=blocking==0?1:0;
        return host_->IoctlSocket(socket_,0x8004667e,value)!=-1;
    }
    int spDXNetwork::SendForAnalysis(const void* data,int size)
    {return socket_==InvalidSocket?-1:host_->Send(socket_,data,size,0);}
    int spDXNetwork::SendToForAnalysis(std::uint32_t address,std::uint16_t port,const void* data,int size)
    {
        if(socket_==InvalidSocket)return -1;
        MakeAddress(address,port);return host_->SendTo(socket_,data,size,0,socketAddress_);
    }
    int spDXNetwork::ReceiveForAnalysis(void* data,int size)
    {return socket_==InvalidSocket?-1:host_->Recv(socket_,data,size,0);}
    int spDXNetwork::ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void* data,int size)
    {
        if(socket_==InvalidSocket)return -1;
        std::uint32_t length=16;
        return host_->RecvFrom(socket_,data,size,0,socketAddress_,length);
    }
    bool spDXNetwork::AddToSetForAnalysis(std::uint32_t selector)
    {
        if(selector>2)return false;
        auto& set=host_->globals.sets[selector];
        if(set.count<64)set.sockets[set.count++]=socket_;
        return true; // Duplicates, INVALID_SOCKET and full sets are accepted.
    }
    bool spDXNetwork::RemoveFromSetForAnalysis(std::uint32_t selector)
    {
        if(selector>2)return false;
        auto& set=host_->globals.sets[selector];
        for(std::uint32_t i=0;i<set.count;++i)
            if(set.sockets[i]==socket_)
            {for(auto j=i+1;j<set.count;++j)set.sockets[j-1]=set.sockets[j];--set.count;break;}
        return true;
    }
    bool spDXNetwork::ClearSetForAnalysis(std::uint32_t selector)
    {if(selector>2)return false;host_->globals.sets[selector].count=0;return true;}
    bool spDXNetwork::SelectForAnalysis(std::uint32_t timeout)
    {
        auto& sets=host_->globals.sets;
        return host_->Select(0,sets[0],sets[1],sets[2],0,SignedWord(timeout*1000u))>0;
    }
    spNetworkAcceptForAnalysis spDXNetwork::AcceptForAnalysis()
    {
        if(socket_==InvalidSocket)return {nullptr,host_->globals.failedAcceptWord};
        spNetworkAddressForAnalysis address;std::uint32_t length=16;
        const auto accepted=host_->Accept(socket_,address,length);
        if(accepted==InvalidSocket)
        {const auto error=host_->LastError();return {nullptr,error==10035?0:host_->globals.failedAcceptWord};}
        auto child=std::make_unique<spDXNetwork>(host_);
        child->socket_=accepted;child->connected_=1;
        // Native copies exactly the returned address length; wider outputs
        // overflow the 16-byte field. Keep that unsafe domain explicit.
        if(length>16)throw std::out_of_range("Native accept address exceeds its 16-byte storage");
        for(unsigned i=0;i<length;++i)
        {child->socketAddress_.bytes[i]=address.bytes[i];child->socketAddress_.knownBytes|=address.knownBytes&static_cast<std::uint16_t>(1u<<i);}
        child->type_=type_;
        child->address_=host_->Ntohl(address.Read(4,4));
        // Original loads four bytes at port+2 before ntohs; upper bits are
        // ignored by WinSock's 16-bit argument.
        child->port_=host_->Ntohs(static_cast<std::uint16_t>(address.Read(2,2)));
        return {std::move(child),0};
    }
    bool spDXNetwork::GetLocalAddressForAnalysis(std::uint32_t& address)
    {
        std::optional<std::string> name;
        (void)host_->GetHostName(name,256);
        if(!name)throw std::logic_error("Original hostname output is unavailable");
        address=host_->Ntohl(host_->GetHostByName(*name));
        if(!localAddressText_)
            localAddressText_=std::to_string(address>>24)+'.'+std::to_string((address>>16)&255)+'.'+
                              std::to_string((address>>8)&255)+'.'+std::to_string(address&255);
        return true;
    }
    const char* spDXNetwork::GetLocalAddressTextForAnalysis()
    {
        if(!localAddressText_){std::uint32_t address;if(!GetLocalAddressForAnalysis(address))return nullptr;}
        return localAddressText_->c_str();
    }
}
