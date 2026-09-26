#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace winx::reconstruction
{
    class wxAlphaManager;
    using wxAlphaColorForAnalysis = std::array<float, 4>;
    // Borrowed adapter views, not native object layouts or reconstructed classes.
    struct wxAlphaMaterialForAnalysis
    {
        std::uint32_t field38 = 0, field34 = 0;
        std::uint32_t* firstLayerMode = nullptr;
    };
    struct wxAlphaRenderableForAnalysis
    {
        wxAlphaMaterialForAnalysis* material = nullptr;
        std::uint32_t field1C = 0;
        std::uint8_t field18 = 0;
    };
    struct wxAlphaPoolObjectForAnalysis
    {
        std::uint16_t flags04 = 0;
        void* field08 = nullptr;
        std::uint8_t active = 0, field2C = 0;
        void* node10 = nullptr;
    };
    struct wxAlphaNodeForAnalysis
    {
        std::uint32_t exactClassID = 0;
        const char* name = nullptr;
        void* parent = nullptr;
        std::vector<void*> children, renderables;
        std::uint32_t* firstComponentFlags = nullptr;
        wxAlphaPoolObjectForAnalysis* data0C = nullptr;
    };
    class wxAlphaManagerHost
    {
    public:
        virtual ~wxAlphaManagerHost() = default;
        virtual wxAlphaPoolObjectForAnalysis* CreatePoolObjectForAnalysis() = 0;
        virtual void DestroyPoolObjectForAnalysis(wxAlphaPoolObjectForAnalysis*) noexcept = 0;
        // Creates/retains the overlay material with native settings: flag6C=1,
        // field38=field34=2, field24=0, one layer with mode2 and default texture.
        virtual void* CreateOverlayMaterialForAnalysis() = 0;
        virtual void ReleaseOverlayMaterialForAnalysis(void*) noexcept = 0;
        virtual std::uint32_t GetGameMillisecondsForAnalysis() noexcept = 0;
        virtual std::uint32_t GetSystemMillisecondsForAnalysis() noexcept = 0;
        virtual void NotifyFadeForAnalysis(void* recipient, wxAlphaManager& sender, std::uint32_t code, std::uint8_t value) noexcept = 0;
        // Draw centered full-viewport black quad, all four vertex alphas equal.
        virtual void DrawOverlayForAnalysis(void* material, float alpha) noexcept = 0;
        virtual wxAlphaNodeForAnalysis& NodeForAnalysis(void*) noexcept = 0;
        virtual wxAlphaRenderableForAnalysis& RenderableForAnalysis(void*) noexcept = 0;
        virtual bool IsRenderableKindOfForAnalysis(void*, std::uint32_t) noexcept = 0;
        virtual wxAlphaColorForAnalysis GetMaterialColorForAnalysis(wxAlphaMaterialForAnalysis&, unsigned getterSlot) noexcept = 0;
        virtual void SetMaterialColorForAnalysis(wxAlphaMaterialForAnalysis&, unsigned setterSlot, const wxAlphaColorForAnalysis&) noexcept = 0;
        virtual void RefreshRenderableForAnalysis(void*) noexcept = 0;
        virtual void* NodeColorTargetForAnalysis(void* node) noexcept = 0;
        virtual wxAlphaColorForAnalysis GetNodeColorForAnalysis(void*) noexcept = 0;
        virtual void SetNodeColorForAnalysis(void*, const wxAlphaColorForAnalysis&) noexcept = 0;
        virtual wxAlphaColorForAnalysis UnpackColorForAnalysis(std::uint32_t) noexcept = 0;
        virtual std::uint8_t ConvertColorByteForAnalysis(double) noexcept = 0;
        // PC 59424D can read an unwritten stack word when no interpolation ran.
        // Supply observed storage explicitly; do not invent a default color.
        virtual std::uint32_t UnwrittenReverseColorForAnalysis() noexcept = 0;
        virtual void ReportFadeableNodeForAnalysis(const char*) noexcept = 0;
    };
}
