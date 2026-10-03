#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxBeforeTrollFightState;
    enum class wxBeforeTrollProfileForAnalysis { PC, PS2 };

    // Borrowed services, not recovered game types. The host and its consumer
    // must outlive every bound state. Removal callbacks cannot throw: they run
    // from the C++ destructor, including when native once flag1D is still set.
    class wxBeforeTrollFightStateHost : public wxCharacterStateHost
    {
    public:
        virtual wxBeforeTrollProfileForAnalysis ProfileForAnalysis() const noexcept = 0;
        virtual std::uint32_t RandomWordForAnalysis() = 0;
        virtual std::uint32_t ClockWordForAnalysis() = 0;
        virtual void AddCompletionSubscriberForAnalysis(void* consumer, wxBeforeTrollFightState& state) = 0;
        virtual void RemoveCompletionSubscriberForAnalysis(void* consumer, wxBeforeTrollFightState& state) noexcept = 0;
        virtual void SubscribeForAnalysis(std::uint32_t key, wxBeforeTrollFightState& state) = 0;
        virtual void UnsubscribeForAnalysis(std::uint32_t key, wxBeforeTrollFightState& state) noexcept = 0;
        // owner24->24, re-read after the node callback.
        virtual void* OwnerNodeForAnalysis(void* owner) = 0;
        virtual void InvokeNodeForAnalysis(void* node, std::uint32_t a, std::uint32_t b) = 0;
        virtual bool NodeHasChildrenForAnalysis(void* node) = 0;
        virtual void* FirstNodeChildForAnalysis(void* node) = 0;
        virtual void WriteChildByteForAnalysis(void* child, std::uint32_t offset, std::uint8_t value) = 0;
        // PC owner124->12C / PS2 owner130->138. The outer object is required;
        // only its inner word is nullable in the original.
        virtual void* OwnerResetObjectForAnalysis(void* owner) = 0;
        // PC4D96A0 remains an external callee, not a successful default stub.
        virtual void ResetPCObjectForAnalysis(void* object) = 0;
        virtual void WritePS2ResetWordForAnalysis(void* object, std::uint32_t offset, std::uint32_t value) = 0;
    };
}
