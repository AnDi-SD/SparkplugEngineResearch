#pragma once

// Inferred file placement. Original class identity and PC algorithm are
// backed by the executable; host pointer/container types are not native ABI.
#include "spTransformEval.h"
#include <functional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTransformTrackEval final : public spTransformEval
    {
      public:
        static constexpr spClassID ClassID = 0x5DAF152D;
        struct PlaybackForAnalysis final
        {
            float weight = 1;
            float time = 0;
            // Optional non-owning view of original actor state +0x48. Required
            // for original insertion; not needed for direct sampling fixtures.
            std::uint32_t* bindingUseCount = nullptr;
        };
        struct KeyCacheForAnalysis final
        {
            std::array<std::int32_t, 3> position{}, rotation{}, scale{};
        };
        // Explicit callable view of 0x00479290; spAnimTrack supplies prepared
        // original key decoding. Borrowed view lifetime is caller-controlled.
        using TrackSamplerForAnalysis =
            std::function<SampleForAnalysis(float, KeyCacheForAnalysis&)>;
        struct InputForAnalysis final
        {
            const PlaybackForAnalysis* playback = nullptr;    // borrowed
            const TrackSamplerForAnalysis* sampler = nullptr; // borrowed
            std::uint32_t priority = 0xFFFFFFFF;
            KeyCacheForAnalysis cache;
        };

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] SampleForAnalysis EvaluateForAnalysis(float time) override;
        // Additional host safety gate; original insertion has no local
        // capacity guard and requires caller invariants not yet reconstructed.
        [[nodiscard]] bool SetInputsForAnalysis(std::vector<InputForAnalysis> inputs);
        // Observation returns an active-prefix copy, not native storage.
        [[nodiscard]] std::vector<InputForAnalysis> GetInputsForAnalysis() const;
        [[nodiscard]] const std::array<InputForAnalysis, 2>& GetPhysicalInputsForAnalysis()
            const noexcept;
        [[nodiscard]] std::size_t GetInputCountForAnalysis() const noexcept;
        // Returns false only for host preflight failure. Native priority rejection
        // is a successful no-op, not a bool-return contract of the original.
        [[nodiscard]] bool InsertInputForAnalysis(InputForAnalysis, bool exclusive);
        [[nodiscard]] bool ClearInputForAnalysis(std::size_t index) noexcept;
        [[nodiscard]] std::int32_t GetBoundSlotForAnalysis() const noexcept;
        void SetBoundSlotForAnalysis(std::int32_t slot) noexcept;

      private:
        friend class spActor; // host preflight remaps copied physical input views
        std::int32_t boundSlot_ = -1;
        std::array<InputForAnalysis, 2> inputs_{};
        std::size_t inputCount_ = 0;
    };
} // namespace sparkplug::reconstruction
