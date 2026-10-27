#pragma once
#include <cstdint>

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    class wxGameTimer;
    class wxPerception;
    class wxHurtAIActionHost
    {
    public:
        virtual ~wxHurtAIActionHost() = default;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        // Foreign command receiver, PC5992E0/PS23730E0 via proven tables
        //703090/494710. Its original native class name remains unknown.
        virtual void ResetCommandForAnalysis(void* command) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(void* command,std::uint32_t offset,std::uint8_t value) noexcept = 0;
        virtual void WriteCommandWordForAnalysis(void* command,std::uint32_t offset,std::uint32_t value) noexcept = 0;
        virtual std::uint32_t ReadCharacterStateForAnalysis(void* owner) noexcept = 0;
        // PC owner24/Entity24/Node and warm profile765AD4/field2B4/Node24.
        // The foreign profile's native class is not attributed by this seam.
        // Nodes are borrowed; this consumer reads LOCAL positions20, not74.
        virtual sparkplug::reconstruction::spNode& ReadOwnerNodeForAnalysis(void* owner) noexcept = 0;
        virtual sparkplug::reconstruction::spNode& ReadPlayerNodeForAnalysis() noexcept = 0;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept = 0;
        virtual wxGameTimer& GameTimerForAnalysis() noexcept = 0;
        // Actual selector entry boundary only; its delivery/body is separate.
        virtual void SelectActionForAnalysis(void* owner,std::uint32_t key,std::uint32_t parameter) noexcept = 0;
    };
}
