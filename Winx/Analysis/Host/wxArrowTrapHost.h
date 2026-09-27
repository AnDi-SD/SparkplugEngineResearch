#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sparkplug::reconstruction { class spCloneManager; }
namespace winx::reconstruction
{
    class wxArrowTrap;
    using wxArrowPosition = std::array<float, 3>;

    // Scene nodes, wxEntity operations, the game clock and the component
    // allocator are outside this recovered leaf.
    class wxArrowTrapHost
    {
    public:
        virtual ~wxArrowTrapHost() = default;
        virtual void ConstructEntityForAnalysis(wxArrowTrap&) = 0;
        virtual void DestroyEntityForAnalysis(wxArrowTrap&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxArrowTrap&, wxArrowTrap&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual void* RootNodeForAnalysis(const wxArrowTrap&) const noexcept = 0;
        virtual std::vector<void*> ChildrenForAnalysis(void*) const = 0;
        virtual std::string_view NameForAnalysis(void*) const noexcept = 0;
        virtual wxArrowPosition PositionForAnalysis(void*) const noexcept = 0;
        virtual void SetPositionAndDirtyForAnalysis(void*, const wxArrowPosition&) noexcept = 0;
        // A nonempty render-data array (PC +68, PS2 +64) wins before the
        // depth-first descendant search. Keep that search in this leaf.
        virtual bool HasRenderDataForAnalysis(void*) const noexcept = 0;
        virtual void* ComponentForAnalysis(void*) const noexcept = 0;
        virtual void* CreateComponentForAnalysis() = 0;
        virtual void AttachComponentForAnalysis(void*, void*) noexcept = 0;
        virtual void EnableComponentFlagForAnalysis(void*, std::uint8_t) noexcept = 0;
        virtual void SetComponentWordForAnalysis(void*, std::uint16_t) noexcept = 0;
        virtual void DestroyComponentForAnalysis(void*) noexcept = 0;
        virtual std::uint32_t RandomForAnalysis() noexcept = 0;
        virtual float DeltaSecondsForAnalysis() noexcept = 0;
        virtual std::uint32_t MillisecondsForAnalysis() noexcept = 0;
        virtual void UpdateEntityTransformForAnalysis(wxArrowTrap&) noexcept = 0;
        virtual void UpdateTargetTransformForAnalysis(wxArrowTrap&, void*) noexcept = 0;
        virtual void NormalizeForAnalysis(wxArrowPosition&) noexcept = 0;
        virtual void SetEntityTimeForAnalysis(wxArrowTrap&, std::uint32_t) noexcept = 0;
        virtual void NotifyForAnalysis(wxArrowTrap&, std::uint32_t code,
            std::uint32_t group, std::uint32_t value18, std::uint32_t value1C) noexcept = 0;
        virtual void AfterSetupForAnalysis(wxArrowTrap&, void*) noexcept = 0;
        virtual void RegisterFloatPropertyForAnalysis(const char*, std::uint32_t pcOffset) = 0;
        virtual void RegisterWordPropertyForAnalysis(const char*, std::uint32_t pcOffset) = 0;
        virtual bool RegisterEntityPropertiesForAnalysis() = 0;
    };
}
