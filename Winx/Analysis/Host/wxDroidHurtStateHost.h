#pragma once
#include "wxCharacterMovementStateHost.h"

namespace winx::reconstruction
{
    class wxDroidHurtState;
    // Our adapter for borrowed owner layout, clock and owner message receiver.
    // State hooks, key construction and runtime fields remain in the class.
    class wxDroidHurtStateHost : public wxCharacterMovementStateHost
    {
    public:
        virtual std::uint8_t ReadHurtControlByteForAnalysis(void* owner) = 0;
        virtual void ClearHurtControlByteForAnalysis(void* owner) = 0;
        virtual std::uint32_t ClockWordForAnalysis() = 0;
        virtual bool HasHurtMessageReceiverForAnalysis(void* owner) = 0;
        struct MessageForAnalysis final
        {
            std::uint32_t code;
            std::array<std::uint32_t, 3> words04;
            const wxDroidHurtState* source;
            std::uint32_t word14;
            std::uint32_t word18;
            std::uint32_t word1C;
        };
        virtual void SendHurtMessageForAnalysis(void* owner, const MessageForAnalysis&) = 0;
    };
}
