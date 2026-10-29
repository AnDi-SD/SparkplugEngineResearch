#pragma once
#include <cstdint>

namespace winx::reconstruction
{
    // Storage-only protocol already used by other action adapters. The owner
    // and all candidate owner144/actor130/command records stay live during a
    // synchronous operation. Each Read is fresh; writes may expose controlled
    // input changes before the next read. No original command class or method
    // is inferred, and no callback chooses the action's outcome/offset/value.
    class wxDroidWanderAIActionHost
    {
    public:
        enum class ProfileForAnalysis {Unqualified,PcStableBorrowedCommandData};
        virtual ~wxDroidWanderAIActionHost()=default;
        virtual ProfileForAnalysis CommandProfileForAnalysis() const noexcept
        {return ProfileForAnalysis::Unqualified;}
        virtual void* ReadCommandForAnalysis(void* owner) noexcept=0;
        virtual void WriteCommandWordForAnalysis(void* command,std::uint32_t offset,std::uint32_t value) noexcept=0;
        virtual void WriteCommandByteForAnalysis(void* command,std::uint32_t offset,std::uint8_t value) noexcept=0;
    };
}
