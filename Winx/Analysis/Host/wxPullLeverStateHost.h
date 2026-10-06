#pragma once
#include "wxOpenGateStateHost.h"
namespace winx::reconstruction
{
    // Borrowed external graph and PC services. Native source names are unknown.
    class wxPullLeverStateHost : public wxOpenGateStateHost
    {
    public:
        // PC owner+124 -> entity+12C -> byte178; PS2 +130/+138/184.
        virtual void SetOwnerEntityControllerFlagForAnalysis(void* owner, bool value) = 0;
        // PC global765AD8 and4E21D0; PS2 global getter369B60.
        virtual void* GlobalPlayerForAnalysis() = 0;
        // PC player+12C ->4D96A0. This foreign PC controller is required.
        // PS2 instead inlines15 controller word clears, recorded separately.
        virtual void ResetPlayerControllerForAnalysis(void* player) = 0;
        // PC global765AD4 -> field2B4; borrowed receiver may be null.
        virtual void* GlobalField2B4ReceiverForAnalysis() = 0;
        // Packet [2737,0,0,0,source,0,flagWord,0]. Only low byte of
        // flagWord is initialized; native upper24 padding remains unknown.
        virtual void SendLeverFlagNotificationForAnalysis(void* receiver,
            wxCharacterState& source, bool active) = 0;
        // PC owner+218; PS2 owner+224. Unsigned nonzero selects first call.
        virtual std::uint32_t OwnerField218ForAnalysis(void* owner) = 0;
        // PC4FACE0 when nonzero,4FAD70 otherwise; PS22B4010/2B3F70.
        virtual void CallOwnerField218BranchForAnalysis(void* owner, bool nonzero) = 0;
    };
}
