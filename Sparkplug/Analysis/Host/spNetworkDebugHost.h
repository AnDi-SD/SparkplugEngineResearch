#pragma once
// Our explicit platform/foreign-object boundary, not recovered engine code.
#include <cstdarg>
#include <cstdint>
#include <memory>
#include <string>

namespace sparkplug::reconstruction
{
    class spNetworkDebug;
    class spNetworkDebugStreamForAnalysis
    {
    public:
        virtual ~spNetworkDebugStreamForAnalysis()=default;
        virtual bool Open(const std::string& path,std::uint32_t mode)=0;
        virtual bool Close() noexcept=0;
        virtual void Seek(std::uint32_t origin,std::uint32_t position)=0;
        virtual void WriteLine(const std::string& text,std::uint32_t argument)=0;
    };
    class spNetworkDebugHost
    {
    public:
        struct EngineClock {std::uint32_t rawTicks,divisor,bias;};
        virtual ~spNetworkDebugHost()=default;
        virtual std::uint32_t TimeGetTime() noexcept=0;
        virtual std::uintptr_t CreateLock()=0;
        // Original Shutdown frees the allocation directly, without calling
        // the lock's destructor/DeleteCriticalSection. Keep that distinction.
        virtual void FreeLockStorage(std::uintptr_t lock) noexcept=0;
        virtual void EnterLock(std::uintptr_t lock)=0;
        virtual void LeaveLock(std::uintptr_t lock) noexcept=0;
        virtual std::string LogDirectory()=0;
        virtual std::unique_ptr<spNetworkDebugStreamForAnalysis> CreateStream()=0;
        virtual EngineClock ReadEngineClock()=0;
        virtual std::uint32_t TimerDivisor()=0; // Native74E050; no engine clock refresh.
        // The imported CRT is external. A result must have a terminator within
        // capacity; native unterminated/truncated output is outside host scope.
        virtual std::string FormatMessage(const char* format,std::va_list arguments,std::uint32_t capacity)=0;
        virtual std::string FormatFrameRate(float value)=0;
        virtual void ConsoleOutput(const std::string& text)=0;
        virtual std::uintptr_t ReadContextField14(std::uintptr_t context)=0;
        virtual void DrawFrameRate(std::uint32_t x,std::uintptr_t target,const std::string& text,std::uint32_t argument)=0;
        virtual void Unsubscribe(spNetworkDebug& object)=0;
        // Packet dump repeatedly resolves engine+70; it need not be `this`.
        virtual spNetworkDebug* PacketDebug()=0;
    };
    // Our exception cleanup for portable host failures. The normal path has
    // the original Enter/Leave order; this does not claim native SEH behavior.
    class spNetworkDebugOutputLockForAnalysis final
    {
    public:
        spNetworkDebugOutputLockForAnalysis(spNetworkDebugHost& host,std::uintptr_t lock):host_(host),lock_(lock)
        {host_.EnterLock(lock_);}
        ~spNetworkDebugOutputLockForAnalysis(){host_.LeaveLock(lock_);}
        spNetworkDebugOutputLockForAnalysis(const spNetworkDebugOutputLockForAnalysis&)=delete;
        spNetworkDebugOutputLockForAnalysis& operator=(const spNetworkDebugOutputLockForAnalysis&)=delete;
    private:
        spNetworkDebugHost& host_;
        std::uintptr_t lock_;
    };
    void SetNetworkDebugFactoryHostForAnalysis(std::shared_ptr<spNetworkDebugHost> host);
    [[nodiscard]] std::shared_ptr<spNetworkDebugHost> GetNetworkDebugFactoryHostForAnalysis();
}
