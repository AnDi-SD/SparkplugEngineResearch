#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    class wxIceWormHolesStateHost : public wxCharacterStateHost
    {
    public:
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        // Message2739: payload byte0 is0 on entry,1 on exit; the other
        // payload word is0. The three padding bytes of the flag word are
        // not initialized by the original. No full32-bit value is inferred.
        virtual void SendFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code, bool flag) = 0;
    };
}
