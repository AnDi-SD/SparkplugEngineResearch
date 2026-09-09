#pragma once

// Inferred source path; original PC RTTI proves the class/base names.
// Partial file-resource ownership/query slice, NOT all33 native slots or
// dynamic render/occlusion placement and Scene visibility initialization.
#include "Code/SparkBase/spBaseObject.h"
#include <array>
#include <vector>

namespace sparkplug::reconstruction
{
    class spZone;
    class spZonePortal;
    class spScene;
    class spPartitionSystem;
    class spPartitionRenderable;
    class spStaticRenderObject;
    class spCollisionInfo;
    class spPartitionNode : public spBaseObject
    {
      public:
        using Vector3 = std::array<float, 3>;
        static constexpr spClassID ClassID = 0x67672341;
        spPartitionNode();
        ~spPartitionNode() override;
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
        // Legacy query fixture seam, independent of actual owned Zone below.
        void SetZonePresentForAnalysis(bool present) noexcept;
        [[nodiscard]] spPartitionNode* GetParentForAnalysis() const noexcept{return parent_;}
        [[nodiscard]] std::uint32_t GetDebugColorForAnalysis() const noexcept{return debugColor_;}
        void SetDebugColorForAnalysis(std::uint32_t value) noexcept{debugColor_=value;}
        void SetZoneForAnalysis(std::shared_ptr<spZone> value) noexcept;
        [[nodiscard]] spZone* GetZoneForAnalysis() const noexcept{return zone_.get();}
        [[nodiscard]] bool AppendPortalForAnalysis(std::shared_ptr<spZonePortal> value);
        [[nodiscard]] const auto& GetPortalsForAnalysis() const noexcept{return portals_;}
        void SetPartitionSystemForAnalysis(spPartitionSystem* system) noexcept;
        [[nodiscard]] spPartitionSystem* GetPartitionSystemForAnalysis() const noexcept{return system_;}
        // Actual4259E0: borrowed scene assignment propagated to current payload,
        // static supports and nonnull children; no Scene creation/registration.
        void SetScenePointerForAnalysis(spScene* scene) noexcept;
        [[nodiscard]] spScene* GetSceneForAnalysis() const noexcept{return scene_;}
        [[nodiscard]] spPartitionRenderable* GetPartitionRenderableForAnalysis() const noexcept{return payload_.get();}
        [[nodiscard]] bool SetPartitionRenderableForAnalysis(std::unique_ptr<spPartitionRenderable> value) noexcept;
        // Native base slots1C/58. Derived spatial insertion is explicitly
        // unsupported here, rather than silently replacing a virtual dispatch.
        [[nodiscard]] virtual bool InsertCollisionForAnalysis(spCollisionInfo* collision);
        [[nodiscard]] virtual bool InsertStaticForAnalysis(std::shared_ptr<spStaticRenderObject> value);
        [[nodiscard]] const auto& GetCollisionsForAnalysis() const noexcept{return collisions_;}
        [[nodiscard]] const auto& GetStaticObjectsForAnalysis() const noexcept{return statics_;}
        void RemoveCollisionForAnalysis(spCollisionInfo* collision,bool notify=true) noexcept;

      protected:
        explicit spPartitionNode(std::size_t childCount);
        [[nodiscard]] bool HasZoneForAnalysis() const noexcept;

      private:
        std::vector<std::unique_ptr<spPartitionNode>> children_;
        bool zonePresent_ = false;
        spPartitionNode* parent_=nullptr;
        std::uint32_t debugColor_=0xFFFFFFFF;
        std::shared_ptr<spZone> zone_;
        std::vector<std::shared_ptr<spZonePortal>> portals_;
        spPartitionSystem* system_=nullptr;
        spScene* scene_=nullptr;
        std::unique_ptr<spPartitionRenderable> payload_;
        std::vector<spCollisionInfo*> collisions_;
        std::vector<std::shared_ptr<spStaticRenderObject>> statics_;
    };
} // namespace sparkplug::reconstruction
