#pragma once

// Original RTTI class, inferred header/source path. PC extent24 is recorded
// separately in Analysis/PC. This is the borrowed scene-list/world slice.
#include "spNode.h"
#include <list>

namespace sparkplug::reconstruction
{
    class spSceneManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x67419388;
        // Explicit view of native scene+14. It is not a reconstructed spScene
        // constructor, subsystem owner, registration graph or rendering API.
        struct SceneForAnalysis { spNode* systemRoot = nullptr; };

        spSceneManager();
        ~spSceneManager() override;
        spSceneManager(const spSceneManager&) = delete;
        spSceneManager& operator=(const spSceneManager&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spSceneManager* GetInstance() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;

        // Stable caller-owned view/root must outlive registration. Native scene
        // ctor/dtor supply these links; duplicate/null/cap guards are host-only.
        [[nodiscard]] bool RegisterSceneForAnalysis(SceneForAnalysis&);
        [[nodiscard]] bool UnregisterSceneForAnalysis(SceneForAnalysis&) noexcept;
        [[nodiscard]] std::size_t GetSceneCountForAnalysis() const noexcept;
        [[nodiscard]] SceneForAnalysis* GetCurrentSceneForAnalysis() const noexcept;
        [[nodiscard]] const std::list<SceneForAnalysis*>& GetScenesForAnalysis() const noexcept;

        // PC45A7D0 calls each root virtual30(0), reading next AFTER the call.
        // Appending/removing a later entry affects this same traversal. Native
        // ignores root return values. Host false aggregates node safety failures
        // without short-circuit; reentry/current removal and >4096 visits guard
        // undefined/unbounded callbacks. Scene/Node Enabled flags are not gates.
        [[nodiscard]] bool UpdateWorldForAnalysis(const spNode::Matrix3* camera = nullptr) noexcept;
    private:
        static spSceneManager* instance_;
        std::list<SceneForAnalysis*> scenes_;
        SceneForAnalysis* currentScene_ = nullptr;
        bool updating_ = false;
    };
}
