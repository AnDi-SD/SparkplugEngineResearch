#pragma once
#include <cstdint>
#include <optional>

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    class wxGameTimer;
    class wxPerception;
    class wxIceGargoyleAIActionHost
    {
    public:
        virtual ~wxIceGargoyleAIActionHost()=default;
        virtual sparkplug::reconstruction::spNode& ReadOwnerNodeForAnalysis(void* owner) noexcept=0;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept=0;
        virtual void WriteCommandWordForAnalysis(void* command,std::uint32_t offset,std::uint32_t value) noexcept=0;
        virtual void WriteCommandByteForAnalysis(void* command,std::uint32_t offset,std::uint8_t value) noexcept=0;
        // PC owner144/character124/word140; this is not state word130.
        virtual std::uint32_t ReadCharacterFlags140ForAnalysis(void* owner) noexcept=0;
        // Literal owner floats1CC and1B4. No broader native entity type is
        // inferred, and their ordering is not assumed by the consumer.
        virtual std::uint32_t ReadOwnerWordForAnalysis(void* owner,std::uint32_t offset) noexcept=0;
        // Entry obtains the identity directly from the real shared manager's
        // CurrentWords[2]. This seam supplies only that foreign record's18
        // data word. nullopt means unknown binding; an engaged null means a
        // known native null Node. Every returned Node is borrowed and stable.
        virtual std::optional<sparkplug::reconstruction::spNode*>
            ReadBorrowedRecordNode18ForAnalysis(std::uintptr_t recordIdentity) noexcept=0;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept=0;
        virtual wxGameTimer& GameTimerForAnalysis() noexcept=0;
        // Actual receiver/key/parameter boundary; selector body is separate.
        virtual void SelectActionForAnalysis(void* owner,std::uint32_t key,std::uint32_t parameter) noexcept=0;
    };
}
