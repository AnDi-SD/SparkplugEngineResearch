#pragma once
// Our OS/global-manager boundary. This is not an original networking backend.
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spNetworkStateCtrlHost
    {
    public:
        using Handle=std::uintptr_t;
        virtual ~spNetworkStateCtrlHost()=default;
        virtual Handle CreateLock()=0;
        // The original frees lock storage without calling its destructor.
        virtual void FreeLockStorage(Handle) noexcept=0;
        virtual void EnterLock(Handle)=0;
        virtual void LeaveLock(Handle) noexcept=0;
        // Each original entry action resolves the current manager separately,
        // including its lazy factory. The returned handle is borrowed.
        virtual Handle ResolveNetworkManager()=0;
        virtual bool StartConnection(Handle)=0; // original004513A0; result ignored
        virtual void ManagerOperation450B70(Handle)=0; // original name unknown
        virtual void ManagerOperation4516F0(Handle,std::uint32_t)=0;
        virtual void RefreshTimer(Handle)=0; // original00450B60
    };
    void SetNetworkStateCtrlFactoryHostForAnalysis(std::shared_ptr<spNetworkStateCtrlHost>);
    [[nodiscard]] std::shared_ptr<spNetworkStateCtrlHost> GetNetworkStateCtrlFactoryHostForAnalysis();
}
