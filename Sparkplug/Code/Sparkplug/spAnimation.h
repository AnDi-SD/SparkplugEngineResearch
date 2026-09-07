#pragma once

// Original class; file path inferred. Physical PC base is spNamedObject while
// engine RTTI says spController. This portable object/track/tag slice is NOT a
// full SAN loader, original allocator/name registry or actor/frame integration.
#include "spAnimTrack.h"
#include <optional>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spAnimation final : public spNamedObject
    {
      public:
        static constexpr spClassID ClassID = 0x56EE563A;
        // Tag has a distinct vtable but inherited root RTTI; no invented class name.
        struct TagForAnalysis
        {
            std::string name;
            float time = 0;
            std::uint32_t wireOrdinal = 0;
        };
        spAnimation() = default;
        spAnimation(const spAnimation&) = delete;
        spAnimation& operator=(const spAnimation&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] float GetTotalTimeForAnalysis() const noexcept
        {
            return totalTime_;
        }
        [[nodiscard]] bool SetTotalTimeForAnalysis(float time) noexcept;
        // PC +18 feeds the high byte of actor input priority at 005A1E30.
        // Original field/API names and the upstream setter remain unknown.
        [[nodiscard]] std::uint32_t GetPriorityGroupForAnalysis() const noexcept
        {
            return priorityGroup_;
        }
        void SetPriorityGroupForAnalysis(std::uint32_t value) noexcept
        {
            priorityGroup_ = value;
        }
        [[nodiscard]] std::size_t GetTrackCountForAnalysis() const noexcept
        {
            return tracks_.size();
        }
        [[nodiscard]] std::size_t GetTrackCapacityForAnalysis() const noexcept
        {
            return trackCapacity_;
        }
        [[nodiscard]] spAnimTrack* GetTrackForAnalysis(std::size_t index) noexcept;
        [[nodiscard]] const spAnimTrack* GetTrackForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] spAnimTrack* AppendTrackForAnalysis();
        [[nodiscard]] bool ResizeTrackCapacityForAnalysis(std::size_t capacity);
        [[nodiscard]] bool InsertTagForAnalysis(TagForAnalysis tag);
        [[nodiscard]] const std::vector<TagForAnalysis>& GetTagsForAnalysis() const noexcept
        {
            return tags_;
        }
        // Explicit dependency value. Native constructor consumes spDebugManager's
        // cycle table; absence here is not falsely represented as a fixed default.
        void SetDebugCycleValueForAnalysis(std::uint32_t value) noexcept
        {
            debugCycleValue_ = value;
        }
        [[nodiscard]] std::optional<std::uint32_t> GetDebugCycleValueForAnalysis() const noexcept
        {
            return debugCycleValue_;
        }

      private:
        float totalTime_ = 0;
        std::uint32_t priorityGroup_ = 0;
        std::size_t trackCapacity_ = 0;
        std::vector<std::unique_ptr<spAnimTrack>> tracks_;
        std::vector<TagForAnalysis> tags_;
        std::optional<std::uint32_t> debugCycleValue_;
    };
} // namespace sparkplug::reconstruction
