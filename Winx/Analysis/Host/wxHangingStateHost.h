#pragma once
#include "wxCharacterMovementStateHost.h"
#include <optional>

namespace winx::reconstruction
{
    // Borrowed objects; offsets are translated explicitly, never ABI casts.
    // PS2Finite requires finite scalar inputs. Native external callees are
    // required services; no successful default implementation stands in for one.
    class wxHangingStateHost : public wxCharacterMovementStateHost
    {
    public:
        virtual void ClearGameManagerWord504ForAnalysis() = 0;
        // PC owner134 / PS2 owner140.
        virtual std::uint32_t OwnerKindForAnalysis(void* owner) = 0;
        // Entity control PC2B4/2BC nodes word78; PS22C0/2C8 word74.
        // Nullopt represents a null FIRST pointer; second is then required.
        virtual std::optional<std::array<float, 2>> EntryComparePairForAnalysis(void* owner) = 0;
        virtual void RotateHangingNodeForAnalysis(void* node, float angle) = 0;
        // Direct owner-control word4 on BOTH PC(owner12C) and PS2(owner138).
        virtual float UpdateMotionForAnalysis(void* owner) = 0;
        // PC entity15C / PS2 entity168.
        virtual float UpdateAngleForAnalysis(void* owner) = 0;
        virtual void SetConsumerSpeedForAnalysis(void* consumer, float speed) = 0;
        virtual void* OwnerEntityControlForAnalysis(void* owner) = 0;
        virtual void WriteEntityFlagForAnalysis(void* control, std::uint8_t value) = 0;
        // PC owner144 / PS2 owner150; preserved as a packed integer word.
        virtual std::uint32_t ExitPackedWordForAnalysis(void* owner) = 0;
        virtual float ExitControlAngleForAnalysis(void* owner) = 0;
        // PC entity-control byte184 / PS2 byte190.
        virtual std::uint8_t ExitEligibilityByteForAnalysis(void* owner) = 0;
        // PC entity158 / PS2 entity164.
        virtual float ExitEntityAngleForAnalysis(void* owner) = 0;
        virtual void WriteDirectControlByte1BForAnalysis(void* owner, std::uint8_t value) = 0;
        // PC4E41C0, PS2 inline actor-control writes: input is captured owner
        // node position, not an invented world-space recomputation.
        virtual void SetFinalControlPositionForAnalysis(void* control, const Vector3& position) = 0;
    };
}
