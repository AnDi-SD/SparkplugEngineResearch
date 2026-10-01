#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxTry2HoistStateHost : public wxCharacterStateHost
    {
    public:
        // Borrowed unsigned word PC owner+218 / PS2 owner+224.
        virtual std::uint32_t OwnerField218ForAnalysis(void* owner) = 0;
        // Nonzero: PC4FACE0 / PS22B4010; zero: PC4FAD70 / PS22B3F70.
        // Native callees receive the owner; their full effects remain external.
        virtual void CallOwnerAfterReleaseForAnalysis(void* owner, bool nonzero) = 0;
    };
}
