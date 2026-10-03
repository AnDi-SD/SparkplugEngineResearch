#pragma once
// Our foreign-object and platform boundary. None of these services substitutes
// for a method owned by spNetworkManager. Handles are borrowed guest identities.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNetworkManager;
    using spNetworkManagerHandleForAnalysis=std::uintptr_t;
    enum class spNetworkManagerServiceForAnalysis {Debug,Server,Peer,Controller,Timer,Matchmaking};
    struct spNetworkManagerNotificationForAnalysis
    {
        std::uint32_t code=0,word04=0,word08=0,word0C=0;
        const spNetworkManager* sender=nullptr;
        std::uint32_t word14=0,word18=0;
        std::uintptr_t word1C=0;
    };
    struct spNetworkManagerPacketForAnalysis
    {
        std::uint16_t type=0;
        std::uint32_t field1=0,field2=0;
        std::vector<std::uint8_t> data;
    };
    class spNetworkManagerHost
    {
    public:
        using Handle=spNetworkManagerHandleForAnalysis;
        using Service=spNetworkManagerServiceForAnalysis;
        struct Timer {std::uint8_t active;std::uint32_t ticks,divisor;};
        virtual ~spNetworkManagerHost()=default;
        virtual Handle CreateQueueLock()=0;
        virtual Handle CreateErrorLock()=0;
        // Both original allocations are freed without DeleteCriticalSection.
        virtual void FreeLockStorage(Handle) noexcept=0;
        virtual void EnterLock(Handle)=0;
        virtual void LeaveLock(Handle) noexcept=0;
        virtual Handle CreateService(Service,spNetworkManager&)=0;
        virtual void DeleteService(Service,Handle) noexcept=0;
        virtual void StartDebug(Handle)=0;
        virtual void SetDebugFile(Handle,std::uint8_t,const std::string&)=0;
        virtual void SetServerQueue(Handle,spNetworkManager&)=0;
        virtual void SetPeerId(Handle,std::uint16_t)=0;
        virtual void Transition(Handle,std::uint32_t,std::uint32_t)=0;
        virtual void PumpController(Handle)=0;
        virtual Handle ControllerLock(Handle)=0;
        virtual std::uint32_t ControllerState(Handle)=0;
        virtual void PumpMatchmaking(Handle)=0;
        virtual Timer ReadTimer(Handle)=0;
        virtual void StartTimer(Handle)=0;
        virtual void RefreshTimer(Handle)=0;
        virtual void SetPeerTimestamp(Handle,std::uint32_t)=0;
        virtual std::uint16_t ServerId(Handle)=0;
        virtual bool OpenServer(Handle)=0;
        virtual bool ConnectPeer(Handle,std::uint32_t)=0;
        virtual void SetServerPeer(Handle,Handle)=0;
        virtual std::int32_t SendPeer(Handle,std::uint16_t destination,std::uint16_t type,
            const std::vector<std::uint8_t>& data,std::uint8_t tcp,
            std::uint32_t field1,std::uint32_t field2,std::uint32_t finalWord)=0;
        virtual const spNetworkManagerPacketForAnalysis& ReadPacket(Handle)=0;
        virtual void DeletePacket(Handle) noexcept=0;
        virtual void ForwardNotification(spNetworkManager&,const void*)=0; // root40F960
        virtual void DispatchError(spNetworkManager&,const spNetworkManagerNotificationForAnalysis&,const std::string&)=0;
        virtual void Log(Handle,const std::string& category,std::int32_t level,
            const std::string& format,const std::vector<std::uint32_t>& arguments)=0;
        virtual std::vector<Handle> Children(spNetworkManager&)=0;
        virtual std::uint32_t ChildClassId(Handle)=0;
        virtual std::uint32_t ChildField2C(Handle)=0;
        virtual void Notify(Handle,const spNetworkManagerNotificationForAnalysis&)=0;
        virtual bool InputChanged(Handle,std::uint32_t key)=0;
        virtual bool InputPressed(Handle,std::uint32_t key)=0;
    };
    void SetNetworkManagerFactoryHostForAnalysis(std::shared_ptr<spNetworkManagerHost>);
    [[nodiscard]] std::shared_ptr<spNetworkManagerHost> GetNetworkManagerFactoryHostForAnalysis();
}
