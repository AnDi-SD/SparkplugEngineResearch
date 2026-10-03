#pragma once
// Our borrowed event/controller/notification boundary, not an original backend.
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxCharacterState;
    class wxFrogAttackStateHost : public wxCharacterStateHost
    {
    public:
        virtual const char* EventTagNameForAnalysis(const void* event)=0;
        virtual void* OwnerField24ForAnalysis(void* owner)=0;
        // PC004FB3A0 / PS2002A6BA0, original animation-controller operation.
        virtual void RestartReverseForAnalysis(void* consumer,void* handle)=0;
        // Packet271F,0,0,0,source,0,0,flag. Only the low flag byte is
        // initialized in the original; upper24 bits remain stack padding.
        virtual void SendUnnamedFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source,bool flag)=0;
    };
}
