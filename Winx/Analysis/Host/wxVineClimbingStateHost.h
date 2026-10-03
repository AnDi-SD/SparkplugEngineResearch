#pragma once
#include "wxCharacterMovementStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    // External game types remain opaque. PC offsets are translated by the
    // host for PS2; this interface is not either native object ABI. PS2Finite
    // requires finite motion, angles and heights (signed zero is supported).
    class wxVineClimbingStateHost : public wxCharacterMovementStateHost
    {
    public:
        virtual std::array<float, 2> OwnerAnglesForAnalysis(void* owner) = 0;
        virtual float OwnerMotionForAnalysis(void* owner) = 0;
        virtual std::uint8_t OwnerControlByte50ForAnalysis(void* owner) = 0;
        virtual void* OwnerEntityControlForAnalysis(void* owner) = 0;
        // PC178 / PS2184, on the entity's control object (not owner12C/138).
        virtual void WriteEntityControlFlagForAnalysis(void* control, std::uint8_t value) = 0;
        virtual std::array<float, 2> ExitHeightsForAnalysis(void* control) = 0;
        // PC513170 / PS22C8B10: original shared turn preparation, not a stub.
        virtual void PrepareTurnForAnalysis(wxCharacterState& state) = 0;
        virtual void ResetEntityControlForAnalysis(void* control) = 0;
        // Native PC4E65A0 receives angle=pi, axis(0,1,0), flags(0,0).
        // PS2 rotation composes matrices; its EE math remains a required host.
        virtual void RotateClimbingNodeForAnalysis(void* node, float angle) = 0;
        virtual Vector3 ReadExitNodeAxisForAnalysis(void* node) = 0;
        // PC4365E0, with the already scaled float32 vector.
        virtual void TranslateExitNodeForAnalysis(void* node, const Vector3& delta) = 0;
        // Required PS2 accumulator path: no PC arithmetic substitution.
        virtual void MovePS2ExitNodeForAnalysis(void* node, float scale) = 0;
        // PC node vslot30 / PS238, argument0.
        virtual void InvokeClimbingNodeForAnalysis(void* node, std::uint32_t argument) = 0;
    };
}
