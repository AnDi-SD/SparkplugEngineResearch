#pragma once
#include "Code/Sparkplug/spFunctionEval.h"

namespace sparkplug::reconstruction { class spNode; }

namespace winx::reconstruction
{
    class wxPerception;
    class wxGameTimer;
    class wxCharacterMoveCtrl;
    // Borrowed character/command data and real common services. No unavailable
    // attack phase, controller reset or pathfinding success callback lives here.
    class wxMinotaurAttackAIActionHost
    {
    public:
        virtual ~wxMinotaurAttackAIActionHost()=default;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept=0;
        virtual wxGameTimer& GameTimerForAnalysis() noexcept=0;
        virtual sparkplug::reconstruction::spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept=0;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept=0;
        virtual std::uint8_t ReadCommandByteForAnalysis(void* command,std::uint32_t offset) noexcept=0;
        virtual void WriteCommandByteForAnalysis(void* command,std::uint32_t offset,std::uint8_t value) noexcept=0;
        virtual void WriteCommandWordForAnalysis(void* command,std::uint32_t offset,std::uint32_t value) noexcept=0;
        // Actor24 -> Entity24 -> actual Node. Geometry bindings remain stable
        // during pure reads/math; position is the common cached WORLD value.
        virtual sparkplug::reconstruction::spNode& ReadNodeForAnalysis(void* actor) noexcept=0;
        virtual std::uint32_t ReadCharacterState130ForAnalysis(void* owner) noexcept=0;
        // Optional typed borrowed owner144/character12C binding. A missing
        // binding remains an explicit refusal, never a successful reset stub.
        virtual wxCharacterMoveCtrl* ReadMoveControllerForAnalysis(void*) noexcept { return nullptr; }
        virtual void SelectActionForAnalysis(void* owner,std::uint32_t key,std::uint32_t parameter) noexcept=0;
    };
}
