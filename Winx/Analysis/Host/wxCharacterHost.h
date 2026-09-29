#pragma once
#include <cstdint>

namespace sparkplug::reconstruction { class spCloneManager; }
namespace winx::reconstruction
{
    class wxCharacter;

    // Adapter for the wxEntity operations outside the recovered character leaf.
    class wxCharacterHost
    {
    public:
        virtual ~wxCharacterHost() = default;
        virtual void ConstructEntityForAnalysis(wxCharacter&) = 0;
        virtual void DestroyEntityForAnalysis(wxCharacter&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxCharacter&, wxCharacter&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual void RegisterCharacterForAnalysis(wxCharacter&, std::uint32_t group) = 0;
        virtual void UnregisterCharacterForAnalysis(wxCharacter&, std::uint32_t group) noexcept = 0;
        virtual void ReleaseReferenceForAnalysis(wxCharacter&,
            std::uint32_t pcOffset, std::uint32_t value) noexcept = 0;
        virtual void DetachCharacterForAnalysis(wxCharacter&) noexcept = 0;
        virtual void AssignEntityReferenceForAnalysis(wxCharacter&, void*) = 0;
        virtual void ClearExternalFlagForAnalysis(wxCharacter&, std::uint32_t mask) = 0;
    };
}
