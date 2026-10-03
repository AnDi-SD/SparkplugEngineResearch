#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    // Our borrowed event/owner adapter, not a substituted state method.
    class wxBirdFlyingStateHost:public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event)=0;
        // Original event_takeoff store: owner byte26C PC, byte278 PS2 =0.
        virtual void ClearTakeoffFlagForAnalysis(void* owner)=0;
    };
}
