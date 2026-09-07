#pragma once

// Inferred path: native PC RTTI proves spLightManager/6FCD243A, but no exact
// source/header path is recovered. Original methods below have analytical names.
#include "spLight.h"
#include <array>
#include <cstddef>
#include <vector>

namespace sparkplug::reconstruction
{
    // Portable non-owning list/selection slice. Native scene registration owns
    // the intrusive links; this host vector preserves their stable list order.
    // No scene is implicitly initialized here. Scene/partition propagation and
    // debug/renderer dispatch are not simulated by this narrower class slice.
    class spLightManager : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0x6FCD243A;
        static constexpr std::size_t OrdinaryLightCapacity = 8;
        using SphereForAnalysis = std::array<float, 4>;

        // Native helper490B20/490B50/490BA0 has no recovered original class
        // name. Its ABI is separate; these host pointers are not Address32.
        class CacheForAnalysis final
        {
          public:
            [[nodiscard]] std::size_t GetCount() const noexcept
            {
                return count_;
            }
            [[nodiscard]] spLight* Get(std::size_t index) const noexcept;
            [[nodiscard]] spLight* GetAmbient() const noexcept
            {
                return ambient_;
            }
            [[nodiscard]] const std::array<spLight*, OrdinaryLightCapacity>& GetRawSlots()
                const noexcept
            {
                return lights_;
            }
            void ResetSelection() noexcept; // original46AC40 preserves unused slots
            void Add(spLight& light) noexcept;
            void Remove(spLight& light) noexcept;

          private:
            std::array<spLight*, OrdinaryLightCapacity> lights_{};
            spLight* ambient_ = nullptr;
            std::size_t count_ = 0;
        };

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Non-owning registration only, corresponding to45A780/45A6B0.
        // Duplicate and4096-cap guards are host-only; native helpers have no
        // membership search/validation. Caller keeps every registered light alive.
        [[nodiscard]] bool RegisterLightForAnalysis(spLight& light);
        [[nodiscard]] bool UnregisterLightForAnalysis(spLight& light) noexcept;
        [[nodiscard]] const std::vector<spLight*>& GetLightsForAnalysis() const noexcept
        {
            return lights_;
        }

        // PC46A850/46A950: Enabled200 is not tested. Inputs should be finite;
        // undefined/nonfinite geometry behavior is not claimed reconstructed.
        [[nodiscard]] static bool IsEligibleForAnalysis(const spLight& light,
                                                        const SphereForAnalysis& worldSphere,
                                                        bool excludeShadowVolumeLights) noexcept;
        void RebuildCacheForAnalysis(CacheForAnalysis& cache, const SphereForAnalysis& worldSphere,
                                     bool excludeShadowVolumeLights = true) const noexcept;
        static void RefreshLightForAnalysis(CacheForAnalysis& cache, spLight& light,
                                            const SphereForAnalysis& worldSphere,
                                            bool excludeShadowVolumeLights = true) noexcept;

        // Explicit borrowed view of scene render nodes at manager+1C. Cache
        // and sphere remain live caller objects. Partition traversal is separate.
        // Duplicate/cap guards are host-only; binding does not attach a Node.
        [[nodiscard]] bool RegisterRenderTargetForAnalysis(CacheForAnalysis& cache,
            const SphereForAnalysis& sphere,bool excludeShadowVolumeLights=true);
        [[nodiscard]] bool UnregisterRenderTargetForAnalysis(CacheForAnalysis& cache) noexcept;
        void RefreshRenderTargetsForAnalysis(spLight& light) noexcept;

      private:
        std::vector<spLight*> lights_;
        struct RenderTargetForAnalysis
        { CacheForAnalysis* cache;const SphereForAnalysis* sphere;bool excludeShadowVolumeLights; };
        std::vector<RenderTargetForAnalysis> renderTargets_;
    };
} // namespace sparkplug::reconstruction
