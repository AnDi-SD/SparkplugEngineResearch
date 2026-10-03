#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    // Borrowed foreign objects, translated offsets, no native ABI casts.
    // PC floating point assumes nearest64 x87; PS2Finite qualifies finite
    // scalar input only. Required services have no successful default stubs.
    enum class wxDefendingNumericProfileForAnalysis { PCNearest64, PS2Finite };
    class wxDefendingStateHost : public wxCharacterStateHost
    {
    public:
        virtual wxDefendingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept = 0;
        // Search owner18 for shield_master/SubMaster, or master for its children.
        virtual void* FindDefendingNodeForAnalysis(void* parent, const char* name,
            bool recursive, bool alternate) = 0;
        virtual void* OwnerNodeSearchRootForAnalysis(void* owner) = 0;
        virtual void InvokeDefendingNodeForAnalysis(void* node, bool enabled, bool propagate) = 0;
        // Position20/24/28, scale30/34/38; dirty word PC B0 / PS2 B4.
        virtual std::uint32_t ReadDefendingNodeWordForAnalysis(void* node, std::uint32_t offset) = 0;
        virtual void WriteDefendingNodeWordForAnalysis(void* node, std::uint32_t offset, std::uint32_t value) = 0;
        virtual std::uint32_t ReadDefendingNodeFlagsForAnalysis(void* node) = 0;
        virtual void WriteDefendingNodeFlagsForAnalysis(void* node, std::uint32_t flags) = 0;
        // PC owner140 / PS2 owner14C, low nibble only.
        virtual std::uint32_t OwnerDefendingPackedWordForAnalysis(void* owner) = 0;
        // PC435D30 / PS2 owner24 virtual0C. Payload words are normalized;
        // native message layout and foreign receiver implementation stay external.
        virtual void SendDefendingMessageForAnalysis(void* owner, const wxCharacterState& sender,
            std::uint32_t code, std::uint32_t word18, std::uint32_t word1C) = 0;
        // Lazy particle manager lookup(node,"ptc") followed by PC48BE50 /
        // PS21BBA30. The service must preserve its own lookup/null behavior.
        virtual void InvokeDefendingParticleForAnalysis(void* node, const char* name) = 0;
        // Shared across instances: PC global765BC4 / PS2 GP-4344.
        virtual std::uint32_t ReadSharedDefendingStageForAnalysis() = 0;
        virtual void WriteSharedDefendingStageForAnalysis(std::uint32_t value) = 0;
        virtual float ReadDefendingDeltaForAnalysis() = 0;
        virtual std::uint32_t ReadDefendingClockForAnalysis() = 0;
        virtual void SetConsumerSpeedForAnalysis(void* consumer, float value) = 0;
        // The update captures direct owner-control BEFORE animation callbacks.
        virtual void* DirectDefendingControlForAnalysis(void* owner) = 0;
        virtual std::uint8_t ReadDefendingHitByteForAnalysis(void* control) = 0;
        virtual void WriteDefendingHitByteForAnalysis(void* control, std::uint8_t value) = 0;
        // PC577E10 / PS22A52A0; original source type/name remain unknown.
        virtual void InvokeDefendingScalarServiceForAnalysis(std::uint32_t word,
            float first, float second, float third) = 0;
    };
}
