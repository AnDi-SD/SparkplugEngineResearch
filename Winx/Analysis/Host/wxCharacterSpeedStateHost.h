#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    // Explicit consumer call boundary: PC4FB6B0 (runtime thunk), PS22A6C20.
    // PS2 writes consumer->field134->field20. The PC thunk is not replaced
    // by a guessed portable consumer layout.
    class wxCharacterSpeedStateHost : public virtual wxCharacterStateHost
    {
    public:
        virtual void SetConsumerSpeedForAnalysis(void* consumer, float speed) = 0;
    };
}
