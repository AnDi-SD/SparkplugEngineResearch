#pragma once
#include "wxCharacterMotionStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    class wxFrogMovingStateHost : public wxCharacterMotionStateHost
    {
    public:
        // Native event+1C -> tag+10 is a borrowed zero-terminated name.
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Native packet words: code,0,0,0,source,0,name,0. All pointers
        // remain borrowed; dispatch is an external receiver virtual call.
        virtual void SendTagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code, const char* name) = 0;
    };
}
