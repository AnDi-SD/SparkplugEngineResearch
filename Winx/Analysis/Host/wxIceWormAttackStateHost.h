#pragma once
#include "wxCharacterStateHost.h"
#include <string_view>
namespace winx::reconstruction
{
    class wxCharacterState;
    // Our required adapter for the borrowed owner graph and event dispatcher.
    class wxIceWormAttackStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner140 / PS2 owner14C: raw packed word, no floating arithmetic.
        virtual std::uint32_t OwnerPackedWordForAnalysis(void* owner) = 0;
        virtual void* OwnerField24ForAnalysis(void* owner) = 0;
        virtual std::string_view EventTagNameForAnalysis(const void* event) = 0;
        // Both packets have zero metadata and source=state. Codes27D1/27D2
        // carry words(6F,0); code2758 carries words(0,0).
        virtual void SendAttackNotificationForAnalysis(void* receiver,
            wxCharacterState& source, std::uint32_t code, std::uint32_t payload) = 0;
    };
}
