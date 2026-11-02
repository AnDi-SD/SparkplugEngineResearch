#pragma once
#include <cstdint>

namespace winx::reconstruction
{
    class wxCharacterMoveCtrl;
    // Structural observations of live borrowed records. These are our data
    // bindings, not native owner/character/command class declarations. The
    // records and returned cells stay live and stable throughout each call;
    // reads do not mutate state, reenter, or supply an operation result.
    class wxMosquitoAttackAIActionHost
    {
    public:
        virtual ~wxMosquitoAttackAIActionHost()=default;
        virtual void* ReadCharacter144ForAnalysis(void* owner) noexcept=0;
        virtual void* ReadCommand130ForAnalysis(void* character) noexcept=0;
        virtual std::uint8_t* BorrowCommandByte1DForAnalysis(void* command) noexcept=0;
        virtual std::uint8_t* BorrowCommandByte20ForAnalysis(void* command) noexcept=0;
        virtual wxCharacterMoveCtrl* ReadController12CForAnalysis(void* character) noexcept=0;
    };
}
