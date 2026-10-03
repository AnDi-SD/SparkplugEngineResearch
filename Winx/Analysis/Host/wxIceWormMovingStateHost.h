#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    enum class wxIceWormMovingNumericProfileForAnalysis { PC, PS2Finite };
    struct wxIceWormMovingObjectsForAnalysis final
    {
        void* motionObject;
        void* turnObject;
    };
    class wxIceWormMovingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner124 -> entity130(motion), entity12C(optional turn).
        // PS2 owner130 -> entity13C(motion), entity138(optional turn).
        virtual wxIceWormMovingObjectsForAnalysis OwnerEntityObjectsForAnalysis(void* owner) = 0;
        virtual void* ReadOwnerTurnObjectForAnalysis(void* owner) = 0;
        virtual float ReadMotionForAnalysis(void* motionObject) = 0;
        // Analytical offsets164/1A0 denote PC fields. PS2 adapter maps them
        // to170/1AC. Both are raw float32 values; ownership stays external.
        virtual float ReadTurnWordForAnalysis(void* turnObject, std::uint32_t offset) = 0;
        // Original external helper PC596F80 / PS22930A0 writes caller float.
        // This required boundary supplies no guessed normalization fallback.
        virtual void NormalizeAngleForAnalysis(float& delta) = 0;
        [[nodiscard]] virtual wxIceWormMovingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept
        { return wxIceWormMovingNumericProfileForAnalysis::PC; }
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Native packet: code,0,0,0,source,0,6E,0. Receiver virtual call is
        // mandatory when nonnull; all pointer fields remain borrowed.
        virtual void SendMovingNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code, std::uint32_t payload) = 0;
    };
}
