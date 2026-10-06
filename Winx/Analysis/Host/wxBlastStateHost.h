#pragma once
#include "wxCharacterStateHost.h"
#include <array>
#include <string_view>
namespace winx::reconstruction
{
    class wxCharacterState;
    enum class wxBlastStatePlatformForAnalysis { PC, PS2 };
    // Our borrowed boundary. Object accessors expose the original foreign
    // pointer reads separately so callback changes preserve the native order.
    class wxBlastStateHost : public wxCharacterStateHost
    {
    public:
        virtual wxBlastStatePlatformForAnalysis PlatformForAnalysis() const noexcept = 0;
        virtual std::string_view EventTagNameForAnalysis(const void* event) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        virtual std::int32_t GlobalModeForAnalysis() = 0;
        virtual void* OwnerResourceObjectForAnalysis(void* owner) = 0;
        virtual bool ConsumeResourceForAnalysis(void* resource, float amount) = 0;
        virtual void* OwnerActionObjectForAnalysis(void* owner) = 0;
        virtual void TriggerActionForAnalysis(void* actionObject, std::uint32_t action) = 0;
        virtual void* SecondaryCharacterForAnalysis() = 0;
        virtual void* CharacterField24ForAnalysis(void* character) = 0;
        virtual void* MainReceiverForAnalysis() = 0;
        // Packet code,0,0,0,source,0,payload[0],payload[1]. Null receivers
        // are skipped by this component, just as by the original sender.
        virtual void SendDirectedNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code,
            const std::array<std::uint32_t, 2>& payload) = 0;
        virtual void SendFilteredNotificationForAnalysis(wxCharacterState& source,
            std::uint32_t code, std::uint32_t filter,
            const std::array<std::uint32_t, 2>& payload) = 0;
    };
}
