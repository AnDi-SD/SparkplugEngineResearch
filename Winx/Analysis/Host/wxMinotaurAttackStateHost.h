#pragma once
#include "wxCharacterSpeedStateHost.h"
namespace winx::reconstruction
{
    enum class wxMinotaurAttackNumericProfileForAnalysis { PC, PS2Finite };
    class wxMinotaurAttackStateHost : public wxCharacterSpeedStateHost
    {
    public:
        // Borrowed object captured BEFORE key/lookup/playback, read afterwards.
        // PC owner124->12C; PS2 owner130->138.
        virtual void* OwnerSpeedObjectForAnalysis(void* owner) = 0;
        virtual float ReadSpeedNumeratorForAnalysis(void* object) = 0;
        virtual float ReadSpeedDenominatorForAnalysis(void* object) = 0;
        // PC owner124->130 byte60; PS2 owner130->13C byte60.
        virtual std::uint8_t OwnerExitFlag60ForAnalysis(void* owner) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Packet271F: payload0 full zero, payload1 LOW byte flag; upper24
        // are original uninitialized stack padding, not established zeros.
        virtual void SendFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, bool flag) = 0;
        virtual wxMinotaurAttackNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept
        { return wxMinotaurAttackNumericProfileForAnalysis::PC; }
    };
}
