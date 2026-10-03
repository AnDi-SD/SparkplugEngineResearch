#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    // Our borrowed owner graph, consumer rate and clock services.
    class wxSpiderHurtStateHost : public wxCharacterStateHost
    {
    public:
        virtual void SetConsumerRateForAnalysis(void* consumer, float value) = 0;
        virtual std::uint32_t ClockWordForAnalysis() = 0;
        // Native PC owner124->object138->object124, PS2 owner130->144->130.
        // All intermediate objects are required by the original call contract.
        virtual void* OwnerPermissionGateForAnalysis(void* owner) = 0;
    };
}
