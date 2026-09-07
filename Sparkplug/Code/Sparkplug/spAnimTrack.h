#pragma once

// Original class name/PC 0x44 extent. Host ownership is deliberately not ABI
// compatible: standalone owner is null instead of native uninitialized bytes,
// prepared snapshots own their data and release is idempotent.
#include "spTrack.h"
#include "spAnimationManager.h"
#include "../../Analysis/PC/spAnimationKeySampling.h"

namespace sparkplug::reconstruction
{
    class spAnimation;
    class spAnimTrack final : public spTrack
    {
      public:
        static constexpr spClassID ClassID = 0x33B61869;
        using TrackDataForAnalysis = evidence::pc::animation_keys::TrackDataForAnalysis;
        using PreparedForAnalysis = evidence::pc::animation_keys::PreparedTrackForAnalysis;
        spAnimTrack() = default;
        spAnimTrack(const spAnimTrack&) = delete;
        spAnimTrack& operator=(const spAnimTrack&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] float GetDurationForAnalysis() const noexcept override;
        void ReleaseKeysForAnalysis() noexcept override;
        [[nodiscard]] bool SetKeysForAnalysis(TrackDataForAnalysis data);
        [[nodiscard]] spTransformTrackEval::TrackSamplerForAnalysis GetSamplerForAnalysis() const;
        // Immutable runtime/prepared arrays used by original SAN writer43DDC0.
        // Host accessor only; no claim about an original public declaration.
        [[nodiscard]] const TrackDataForAnalysis* GetKeysForAnalysis() const noexcept
        {
            return prepared_ ? &prepared_->Data() : nullptr;
        }
        [[nodiscard]] spAnimation* GetOwnerForAnalysis() const noexcept
        {
            return owner_;
        }
        [[nodiscard]] std::int32_t GetBindingSlotForAnalysis() const noexcept
        {
            return bindingSlot_;
        }
        void SetBindingSlotForAnalysis(std::int32_t slot) noexcept
        {
            bindingLease_.reset(); // manual borrowed slot replaces any owned reference
            bindingSlot_ = slot;
        }
        [[nodiscard]] bool BindNameForAnalysis(spAnimationManager& manager);
        [[nodiscard]] bool IsBoundToForAnalysis(const spAnimationManager& manager) const noexcept;

      private:
        friend class spAnimation;
        spAnimation* owner_ = nullptr;
        std::int32_t bindingSlot_ = -1;
        std::optional<spAnimationManager::NameBindingLeaseForAnalysis> bindingLease_;
        std::shared_ptr<const PreparedForAnalysis> prepared_;
    };
} // namespace sparkplug::reconstruction
