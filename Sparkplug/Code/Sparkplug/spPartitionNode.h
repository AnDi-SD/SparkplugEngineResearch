#pragma once

// Inferred source path; original PC RTTI proves the class/base names.
// Partial child ownership + query slice, NOT all33 native slots or the
// collision/render/occlusion/Zone intrusive ownership implementation.
#include "Code/SparkBase/spBaseObject.h"
#include <array>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPartitionNode : public spBaseObject
    {
      public:
        using Vector3 = std::array<float, 3>;
        static constexpr spClassID ClassID = 0x67672341;
        spPartitionNode() = default;
        ~spPartitionNode() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        [[nodiscard]] virtual spPartitionNode* FindLeafForAnalysis(const Vector3& point,
                                                                   bool stopAtZone = true) noexcept;
        [[nodiscard]] std::size_t GetChildCountForAnalysis() const noexcept;
        [[nodiscard]] spPartitionNode* GetChildForAnalysis(std::size_t index) noexcept;
        // Host-only bounded setup, NOT recovered native setter. Children are
        // direct-owned in native too; unique_ptr is not a 32-bit ABI model.
        [[nodiscard]] bool SetChildForAnalysis(std::size_t index,
                                               std::unique_ptr<spPartitionNode> child) noexcept;
        // Only the presence used by query40 is represented, not a real Zone
        // object/reference count or automatic Scene registration.
        void SetZonePresentForAnalysis(bool present) noexcept;

      protected:
        explicit spPartitionNode(std::size_t childCount);
        [[nodiscard]] bool HasZoneForAnalysis() const noexcept;

      private:
        std::vector<std::unique_ptr<spPartitionNode>> children_;
        bool zonePresent_ = false;
    };
} // namespace sparkplug::reconstruction
