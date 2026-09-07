#pragma once

// Inferred path; PC registration proves the original class name/ID/base.
// Partial PC46D270/46C4D0/46B870 selection slice only. Native constructor
// preparation46C0F0, graph traversal, portals, occluders and GPU dispatch are
// NOT implemented by this portable record-oriented interface.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/PC/spVisibilityMath.h"

namespace sparkplug::reconstruction
{
    class spVisibilityManager : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0x3D7F4387;
        using PlaneSetForAnalysis = evidence::pc::visibility_math::PlaneSet;
        enum class SupportKindForAnalysis
        {
            Partition,
            Static,
            RenderNode
        };
        // Borrowed test/adapter view, NOT original storage or an original
        // named class. Caller supplies the current native-equivalent fields.
        struct SupportForAnalysis
        {
            SupportKindForAnalysis kind = SupportKindForAnalysis::Static;
            evidence::pc::visibility_math::Sphere worldSphere{};
            std::uint32_t visibilityMark = 0;
            bool enabled = true;
            bool cullBypass = false;
        };

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        void BeginFrameForAnalysis(std::uint32_t& sceneStamp) noexcept;
        // Invoke in native payload -> static -> dynamic order. No implicit
        // Scene/partition walk is claimed. Accepted records stay borrowed.
        [[nodiscard]] bool TrySubmitForAnalysis(SupportForAnalysis& support,
                                                const PlaneSetForAnalysis& planes,
                                                bool unclippedDebug = false);
        void SetSphereCullingForAnalysis(bool value) noexcept
        {
            sphereCulling_ = value;
        }
        void SetFrameStampForAnalysis(std::uint32_t value) noexcept
        {
            frameStamp_ = value;
        }
        [[nodiscard]] std::uint32_t GetFrameStampForAnalysis() const noexcept
        {
            return frameStamp_;
        }
        [[nodiscard]] const std::vector<SupportForAnalysis*>& GetVisibleForAnalysis() const noexcept
        {
            return visible_;
        }

      private:
        std::uint32_t frameStamp_ = 0;
        bool sphereCulling_ = true;
        std::vector<SupportForAnalysis*> visible_;
    };
} // namespace sparkplug::reconstruction
