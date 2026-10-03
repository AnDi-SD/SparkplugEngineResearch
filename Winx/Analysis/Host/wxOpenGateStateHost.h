#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    class wxOpenGateStateHost : public wxCharacterStateHost
    {
    public:
        // Native event+1C -> tag+10; borrowed zero-terminated string.
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        // PC owner+124; PS2 owner+130. Borrowed pointer, possibly null.
        virtual void* OwnerField124ForAnalysis(void* owner) = 0;
        // PC40EC00/PS21007A0 builds packet [code,0,0,filter,source,0,p0,p1]
        // and calls the external message service. Filter meanings stay open.
        virtual void SendFilteredNotificationForAnalysis(wxCharacterState& source,
            std::uint32_t code, std::uint32_t filter, void* payload0,
            std::uint32_t payload1) = 0;
    };
}
