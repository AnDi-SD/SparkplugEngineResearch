#pragma once
#include "Analysis/Host/wxIceGargoyleAIActionHost.h"
#include "Code/Sparkplug/spFunctionEval.h"
#include <array>

namespace winx::reconstruction
{
    // Portable data/service interface reuse, independent of native action
    // inheritance. The action remains a direct wxAIAction sibling.
    class wxIceGargoyleAttackAIActionHost:public wxIceGargoyleAIActionHost
    {
    public:
        // Mutable borrowed PC7600E0 data, read once by this own constructor.
        virtual std::array<std::uint32_t,3> ReadDefaultVectorWordsForAnalysis() noexcept=0;
        // PC742BF4..742C20, one selected three-word record. This interface
        // supplies raw data; original global/vector semantic names are open.
        virtual std::array<std::uint32_t,3> ReadWaypointWordsForAnalysis(unsigned index) noexcept=0;
        // PC740328; bounded orientation admits the actual canonical {0,1,0}.
        virtual std::array<std::uint32_t,3> ReadUpVectorWordsForAnalysis() noexcept=0;
        // Proven foreign command703090 slot04 ->5992E0; native class unknown.
        virtual void ResetCommandForAnalysis(void* command) noexcept=0;
        virtual std::uint32_t ReadCharacterState130ForAnalysis(void* owner) noexcept=0;
        // Foreign character138 receiver identity only. Non-null delivery is
        // explicitly unqualified rather than implemented as successful no-op.
        virtual void* ReadCharacterBrain138ForAnalysis(void* owner) noexcept=0;
        virtual sparkplug::reconstruction::spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept=0;
    };
}
