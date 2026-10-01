#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxMosquitoMovingStateHost : public wxCharacterStateHost
    {
    public:
        // Float at control+4; PC owner+12C / PS2 owner+138.
        virtual float OwnerMotionForAnalysis(void* owner) = 0;
    };
}
