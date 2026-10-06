#include "spPCThread.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        template<class Callback>
        Callback& Require(host::spPCThreadHost* provider, Callback host::spPCThreadHost::*member)
        {
            if (!provider || !(provider->*member))
                throw std::runtime_error("spPCThread requires its explicit OS host service");
            return provider->*member;
        }
    }
    const spRTTIRecord& spPCThread::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spThread::ClassID,
            "spPCThread", &spThread::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spPCThread>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spPCThread::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spPCThread::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spPCThread>();
        manager.RegisterCloneForAnalysis(*this, *result);
        return spNamedObject::vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    spPCThread::StateForAnalysis spPCThread::GetStateForAnalysis() const noexcept
    {
        return {bodyParameter_, opaqueByte_, handle_};
    }
    void spPCThread::SetStateForAnalysis(StateForAnalysis state) noexcept
    {
        bodyParameter_ = state.bodyParameter;
        opaqueByte_ = state.opaqueByte;
        handle_ = state.handle;
    }
    bool spPCThread::CreateForAnalysis(std::uint32_t entryToken, std::uint32_t bodyParameter)
    {
        auto& create = Require(host_, &HostForAnalysis::create);
        bodyParameter_ = bodyParameter;
        handle_ = create({entryToken, this, 4, 0});
        return handle_ != 0;
    }
    bool spPCThread::WaitForAnalysis(std::uint32_t milliseconds)
    {
        if (!handle_)
            return true;
        return Require(host_, &HostForAnalysis::wait)(handle_, milliseconds) != 0x102;
    }
    bool spPCThread::IsRunningForAnalysis()
    {
        if (!handle_)
            return false;
        const auto observation = Require(host_, &HostForAnalysis::exitCode)(handle_);
        if (!observation.code)
            throw std::runtime_error("spPCThread exit-code out-cell residue was not observed");
        if (*observation.code == 0x103)
            return true;
        handle_ = 0;
        return false;
    }
    bool spPCThread::ResumeForAnalysis()
    {
        if (!handle_)
            return false;
        auto& resume = Require(host_, &HostForAnalysis::resume);
        opaqueByte_ = 0;
        (void)resume(handle_);
        return true;
    }
    bool spPCThread::SuspendForAnalysis()
    {
        if (!handle_)
            return false;
        (void)Require(host_, &HostForAnalysis::suspend)(handle_);
        return true;
    }
    void spPCThread::SleepForAnalysis(std::uint32_t milliseconds)
    {
        Require(host_, &HostForAnalysis::sleep)(milliseconds);
    }
    bool spPCThread::TerminateForAnalysis(std::uint32_t exitCode)
    {
        if (!handle_)
            return false;
        return Require(host_, &HostForAnalysis::terminate)(handle_, exitCode) == 1;
    }
}
