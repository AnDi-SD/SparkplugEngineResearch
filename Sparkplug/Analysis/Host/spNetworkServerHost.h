#pragma once
// Foreign platform/manager storage only. Own Server operations execute in
// spNetworkServer; this interface supplies no default successful services.
#include "../../Code/SparkBase/spSocketStream.h"
#include <memory>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNetworkServer;
    class spNetworkPacket;
    class spNetworkPeer;
    class spNetworkServerHost
    {
    public:
        using Handle=std::uintptr_t;
        struct Timer {std::uint8_t active;std::uint32_t ticks,divisor;};
        virtual ~spNetworkServerHost()=default;
        virtual Handle CreatePeerLock()=0;
        // Native frees storage without calling DeleteCriticalSection.
        virtual void FreeLockStorage(Handle) noexcept=0;
        virtual void EnterLock(Handle)=0;
        virtual void LeaveLock(Handle) noexcept=0;
        virtual Handle CreateThread()=0;
        // Native deleting call uses the secondary object subobject at +4.
        virtual void DeleteThread(Handle) noexcept=0;
        virtual bool ThreadCreate(Handle,std::uint32_t entryToken,spNetworkServer& bodyParameter)=0;
        virtual bool ThreadResume(Handle)=0;
        virtual bool ThreadIsRunning(Handle)=0;
        virtual void SetThreadStopByte(Handle,std::uint8_t)=0;
        virtual bool ThreadWait(Handle,std::uint32_t milliseconds)=0;
        virtual std::uint8_t ReadThreadStopByte(Handle)=0;
        // A concrete provider may delegate these to the existing spPCThread;
        // no OS-thread policy is duplicated in the network component.
        virtual std::unique_ptr<spSocketStream> CreateSocketStream()=0;
        virtual Handle ResolveNetworkDebug()=0;
        // Native admission count diagnostic reads a separate global word.
        virtual std::int32_t AdmissionLogLevel()=0;
        virtual void Log(Handle,std::int32_t,const std::string& category,
            const std::string& format,const std::vector<std::uint32_t>& arguments)=0;
        virtual void ManagerError(std::uint32_t)=0;
        virtual void ManagerTransition(std::uint32_t,std::uint8_t)=0;
        virtual Timer ReadManagerTimer()=0;
        virtual void StartManagerTimer()=0;
        virtual void RefreshManagerTimer()=0;
        // Each read includes the current controller's own lock/unlock boundary.
        virtual std::uint32_t ReadManagerControllerState()=0;
        virtual void IncrementManagerStatistic(std::size_t index)=0;
        virtual std::unique_ptr<spNetworkPacket> CreatePacket()=0;
        // Native caller passes the freshly resolved debug, including null.
        virtual void DumpPacket(Handle,const spNetworkPacket&)=0;
        // Foreign queue insertion consumes the packet; same queue representation
        // and lock/list contract as the existing NetworkManager queue adapter.
        virtual void EnqueuePacket(Handle,std::unique_ptr<spNetworkPacket>)=0;
        // Original drops this pointer when no queue is attached. Our mandatory
        // analytical sink retains that allocation explicitly across Server
        // lifetime; it must not silently delete or deliver it as a queued item.
        virtual void RetainUnqueuedPacket(std::unique_ptr<spNetworkPacket>)=0;
        virtual std::unique_ptr<spNetworkPeer> CreatePeer()=0;
        // Adoption failure leaks the new Peer in native. Retain the actual
        // portable owner, including its adopted TCP stream, explicitly.
        virtual void RetainUnacceptedPeer(std::unique_ptr<spNetworkPeer>)=0;
        // Original SetQueue deletes the OLD queue object, including lock,
        // list nodes/sentinel and wrapper; queued packet objects are not freed.
        // Final queue is not deleted by the Server destructor.
        virtual void DeleteQueue(Handle)=0;
    };
    void SetNetworkServerFactoryHostForAnalysis(std::shared_ptr<spNetworkServerHost>);
    [[nodiscard]] std::shared_ptr<spNetworkServerHost> GetNetworkServerFactoryHostForAnalysis();
}
