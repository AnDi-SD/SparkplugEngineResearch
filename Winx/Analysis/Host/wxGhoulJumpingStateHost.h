#pragma once
#include "wxFrogJumpingStateHost.h"
namespace winx::reconstruction
{
    // Our borrowed projection of direct owner action-control fields.
    struct wxGhoulJumpActionControlForAnalysis final
    {
        float field4 = 0;
        std::uint8_t flag51 = 0;
    };
    class wxGhoulJumpingStateHost : public wxFrogJumpingStateHost
    {
    public:
        // PC owner+12C; PS2 owner+138. Retain the view for the entry call.
        virtual wxGhoulJumpActionControlForAnalysis& OwnerDirectActionControlForAnalysis(void* owner) = 0;
        // PC owner+218, PS2 owner+224; unsigned nonzero chooses first call.
        virtual std::uint32_t OwnerField218ForAnalysis(void* owner) = 0;
        // Foreign PC4FACE0/4FAD70, PS22B4010/2B3F70. Names are unknown.
        virtual void CallOwnerField218BranchForAnalysis(void* owner, bool nonzero) = 0;
    };
}
