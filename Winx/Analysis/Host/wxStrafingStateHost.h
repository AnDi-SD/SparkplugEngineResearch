#pragma once
#include "wxCharacterStateHost.h"
#include <array>

namespace winx::reconstruction
{
    class wxCharacterState;
    enum class wxStrafingNumericProfileForAnalysis { PC, PS2Finite };
    class wxStrafingStateHost : public wxCharacterStateHost
    {
    public:
        using Vector3 = std::array<float, 3>;
        virtual wxStrafingNumericProfileForAnalysis NumericProfileForAnalysis() const noexcept = 0;
        // Borrowed entity cached observer: PC owner124/entity154,
        // PS2 owner130/entity160. Native helper is called with create=false.
        virtual void* ReadCachedStrafingObserverForAnalysis(void* owner) = 0;
        virtual void* ReadObserverBindingForAnalysis(void* observer) = 0;
        virtual void WriteObserverBindingForAnalysis(void* observer, void* binding) = 0;
        virtual void* ReadStrafingEntityControlForAnalysis(void* owner) = 0;
        virtual void RemoveStrafingObserverForAnalysis(void* binding, void* observer) = 0;
        virtual void AddStrafingObserverForAnalysis(void* binding, void* observer) = 0;
        virtual void WriteStrafingObserverWordForAnalysis(void* observer, std::uint32_t offset, std::uint32_t value) = 0;
        virtual void WriteStrafingObserverByte64ForAnalysis(void* observer, std::uint8_t value) = 0;
        // PC entity148 / PS2 entity154, tests for null only.
        virtual bool HasStrafingEntityField148ForAnalysis(void* owner) = 0;
        // PC entity130 / PS2 entity13C. Update captures this object once.
        virtual void* ReadStrafingMotionObjectForAnalysis(void* owner) = 0;
        virtual float ReadStrafingMotionWordForAnalysis(void* object, std::uint32_t offset) = 0;
        virtual void SetConsumerSpeedForAnalysis(void* consumer, float speed) = 0;
        virtual void WriteStrafingDirectMotionForAnalysis(void* owner, float value) = 0;
        // Native boolean initializes one byte of payload18; high bytes are
        // unspecified, not fabricated zero padding by this normalized adapter.
        virtual void SendStrafingMessageForAnalysis(void* owner, const wxCharacterState& sender,
            std::uint32_t code, bool enabled, std::uint32_t word1C) = 0;
        // PC596F80 / PS22930A0 operates on a caller-owned float32.
        virtual void NormalizeStrafingAngleForAnalysis(float& angle) = 0;
        // PC owner24/node24 basis8C or A4; PS2 90 or A8. Row0/row2.
        // PC captures owner after angle normalization, then rereads node for
        // each row. PS2 reloads owner for each row; valid bindings stay stable
        // during these pure geometry services.
        virtual Vector3 ReadStrafingBasisForAnalysis(void* owner, unsigned row) = 0;
        // PC41D2D0, PS2 inline EE normalization. Shared math service is
        // required; the PS2 host must implement its own accumulator/SQRT
        // profile, not silently replace it with PC or generic MIPS math.
        virtual void NormalizeStrafingBasisForAnalysis(Vector3& value) = 0;
        // PC597140/5971A0, PS2292F20/292DB0. Original source names unknown.
        virtual float InvokeStrafingVectorAngleForAnalysis(const Vector3& value) = 0;
        virtual float InvokeStrafingAnglePairForAnalysis(float basisAngle, float motionAngle) = 0;
    };
}
