#pragma once
// Our borrowed event/entity-controller boundary, not an original backend.
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxMosquitoAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event)=0;
        // Required owner+124 -> entity+12C PC reset service4D96A0.
        // PS2 owner+130 -> entity+138 has fifteen inline zero-word writes.
        virtual void ResetOwnerEntityControllerForAnalysis(void* owner)=0;
        // Required owner+124 -> entity+140 -> vslot38(0) on PC;
        // PS2 owner+130 -> entity+14C -> vslot40(0). Source name unknown.
        virtual void CallOwnerEntityField140Slot38ForAnalysis(void* owner,bool value)=0;
    };
}
