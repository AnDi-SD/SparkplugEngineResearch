#pragma once
#include "wxCharacterStateHost.h"
#include <array>
#include <stdexcept>

namespace winx::reconstruction
{
    enum class wxCharacterMovementNumericProfileForAnalysis { PCFinite, PS2Finite };
    class wxCharacterMovementStateHost : public wxCharacterStateHost
    {
    public:
        using Vector3 = std::array<float, 3>;
        using Matrix3 = std::array<float, 9>;
        // PC owner18 object's vslot2C / PS2 vslot30; borrowed result.
        virtual void* FindMovementNodeForAnalysis(void* owner, const char* name,
            bool recursive, bool lastArgument) = 0;
        virtual float ReadNodePositionWordForAnalysis(void* node, std::uint32_t offset) = 0;
        virtual void WriteNodePositionWordForAnalysis(void* node, std::uint32_t offset, float value) = 0;
        // OR1 into PC nodeB0 / PS2 nodeB4; preserve all other flags.
        virtual void MarkMovementNodeDirtyForAnalysis(void* node) = 0;
        // Both platforms owner24 -> object's24. All nodes remain borrowed.
        virtual void* OwnerMovementTargetForAnalysis(void* owner) = 0;
        virtual Matrix3 ReadMovementTargetOrientationForAnalysis(void* node) = 0;
        [[nodiscard]] virtual wxCharacterMovementNumericProfileForAnalysis MovementNumericProfileForAnalysis() const noexcept
        { return wxCharacterMovementNumericProfileForAnalysis::PCFinite; }
        // Required separate EE accumulator service for the rotated PS2 path.
        // The generic PC math is never substituted for the original EE body.
        virtual Vector3 TransformPS2MovementDeltaForAnalysis(const Vector3&, const Matrix3&)
        { throw std::logic_error("PS2 rotated movement requires a qualified EE accumulator adapter"); }
    };
}
