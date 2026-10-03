#pragma once
#include "wxCharacterMovementStateHost.h"
namespace winx::reconstruction
{
    struct wxStickingObjectsForAnalysis final
    {
        void* angleObject;
        void* controlObject;
    };
    class wxStickingStateHost : public wxCharacterMovementStateHost
    {
    public:
        // PC owner13C/144, PS2 owner148/150. Packed words, original types open.
        virtual std::uint32_t ReadEntryOwnerWordForAnalysis(void* owner) = 0;
        virtual std::uint32_t ReadExitOwnerWordForAnalysis(void* owner) = 0;
        // PC owner124/12C, PS2 owner130/138. Capture both borrowed objects.
        virtual wxStickingObjectsForAnalysis CaptureStickingObjectsForAnalysis(void* owner) = 0;
        // PC angle-object15C / PS2 angle-object168; control4 on both.
        virtual float ReadStickingAngleForAnalysis(void* object) = 0;
        virtual float ReadStickingMotionForAnalysis(void* object) = 0;
        // Original shared speed wrapper PC512F00 / PS22C8CB0.
        virtual void SetConsumerSpeedForAnalysis(void* consumer, float speed) = 0;
    };
}
