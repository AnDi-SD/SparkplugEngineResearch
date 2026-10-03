#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxStrugglingStateHost : public wxCharacterStateHost
    {
    public:
        // Borrowed owner+124 -> controller+12C on PC (PS2+130 -> +138).
        // PC4D96A0 is an external protected reset service. PS2 inlines its
        // fifteen zero-word writes. This adapter must perform the service;
        // no successful default or invented physical controller type.
        virtual void ResetOwnerEntityControllerForAnalysis(void* owner) = 0;
    };
}
