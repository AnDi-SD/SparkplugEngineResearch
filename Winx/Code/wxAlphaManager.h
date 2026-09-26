#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAlphaManagerHost.h"
#include <array>

namespace winx::reconstruction
{
    class wxAlphaManager : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x41F81774;
        static constexpr std::size_t EffectCount = 200, PoolCount = 150;
        struct EffectForAnalysis
        {
            std::uint32_t mode = 1;
            void* node = nullptr;
            void* renderable = nullptr;
            std::uint8_t active = 0;
            std::uint32_t lifetimeStarted = ~0u, interpolationStarted = 0;
            // Native constructor leaves colors/durations/backups untouched.
            // Portable zeros are storage only; they are not native defaults.
            std::uint32_t from = 0, to = 0, current = 0, duration = 0;
            std::uint8_t saved = 0;
            std::uint32_t lifetime = 0, saved38 = 0, saved34 = 0;
            wxAlphaColorForAnalysis savedColor10{}, savedColor08{}, savedNodeColor{};
            std::uint32_t saved1C = 0;
            std::uint8_t saved18 = 0;
            std::uint32_t savedLayerMode = 0;
        };
        struct FadeForAnalysis
        {
            std::uint8_t enabled = 0, notified = 1;
            float from = 1, to = 0;
            std::uint32_t started = 0, duration = 1000;
            void* field35C = nullptr;
            void* recipient = nullptr;
        };
        explicit wxAlphaManager(wxAlphaManagerHost&);
        ~wxAlphaManager() override;
        wxAlphaManager(const wxAlphaManager&) = delete;
        wxAlphaManager& operator=(const wxAlphaManager&) = delete;
        static void SetFactoryHostForAnalysis(wxAlphaManagerHost*) noexcept;
        static wxAlphaManager* GetInstanceForAnalysis() noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        // Copy and notifications inherit the original no-op spBaseObject hooks.
        void ClearForAnalysis() noexcept;
        void ResetForAnalysis();
        wxAlphaPoolObjectForAnalysis* AcquirePoolObjectForAnalysis();
        void InitializeOverlayForAnalysis();
        void NotifyFadeForAnalysis(std::uint8_t) noexcept;
        void FadeToTransparentForAnalysis(std::uint32_t duration, void* recipient);
        void FadeToOpaqueForAnalysis(std::uint32_t duration, void* recipient);
        void DrawFadeForAnalysis() noexcept;
        void SetField35CForAnalysis(void* p) noexcept { fade_.field35C = p; }
        bool RestoreEffectForAnalysis(EffectForAnalysis*) noexcept;
        bool UpdateEffectForAnalysis(EffectForAnalysis*) noexcept;
        void UpdateEffectsForAnalysis();
        // PC protected type-check seam is confirmed independently by PS2.
        void ApplyToNodeForAnalysis(void*, const std::array<float,3>&, std::uint32_t duration, std::uint32_t lifetime);
        void ApplyRecursiveForAnalysis(void*, const std::array<float,3>&, std::uint32_t duration, std::uint32_t lifetime);
        void FlashRecursiveForAnalysis(void*, std::uint32_t duration, const std::array<float,3>& byteColor);
        void SetColorRecursiveForAnalysis(void*, const std::array<float,3>&);
        void ClassifyRecursiveForAnalysis(void*);
        EffectForAnalysis& GetEffectForAnalysis(std::size_t i) { return effects_.at(i); }
        const FadeForAnalysis& GetFadeForAnalysis() const noexcept { return fade_; }
    private:
        void RequireLive() const;
        void BeginFade(float from, float to, std::uint32_t duration, void* recipient);
        wxAlphaManagerHost& host_;
        std::array<EffectForAnalysis, EffectCount> effects_{};
        std::vector<wxAlphaPoolObjectForAnalysis*> pool_;
        FadeForAnalysis fade_;
        void* overlay_ = nullptr;
        std::array<float,3> flashColor_{220,20,60};
        bool cleared_ = false;
    };
}
