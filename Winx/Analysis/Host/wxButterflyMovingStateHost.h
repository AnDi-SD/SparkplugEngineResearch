#pragma once
#include "wxCharacterStateHost.h"
#include <array>

namespace winx::reconstruction
{
    // PC-only borrowed services. The PS2 entry and movement use unqualified
    // EE accumulator instructions and are deliberately not a numeric profile.
    class wxButterflyMovingStateHost : public wxCharacterStateHost
    {
    public:
        using Vector3 = std::array<float, 3>;
        virtual void* ButterflyNodeForAnalysis(void* owner) = 0;
        virtual float ReadButterflyNodeWordForAnalysis(void* node, unsigned offset) = 0;
        virtual void WriteButterflyNodeWordForAnalysis(void* node, unsigned offset, float value) = 0;
        virtual void MarkButterflyNodeDirtyForAnalysis(void* node) = 0;
        virtual void InvokeButterflyNodeForAnalysis(void* node, bool argument) = 0;
        virtual unsigned ButterflyRandomForAnalysis() = 0;
        // PC5972F0. Output aliases the state's own target vector.
        virtual void ButterflyRandomPointForAnalysis(Vector3& output, float radius, Vector3 origin) = 0;
        virtual unsigned ButterflyClockForAnalysis() = 0;
        virtual float ButterflyDeltaForAnalysis() = 0;
    };
}
