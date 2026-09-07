#include "spSceneManager.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    spSceneManager* spSceneManager::instance_ = nullptr;
    spSceneManager::spSceneManager() { instance_ = this; }
    spSceneManager::~spSceneManager()
    {
        instance_ = nullptr; // PC45ACA6 is unconditional, including old instances.
        // list owns its entries only; scene views and roots remain caller-owned.
    }
    const spRTTIRecord& spSceneManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "spSceneManager",
            &spBaseObject::StaticRTTI(), +[]() -> std::unique_ptr<spBaseObject> {
                return std::make_unique<spSceneManager>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    spSceneManager* spSceneManager::GetInstance() noexcept { return instance_; }
    const spRTTIRecord& spSceneManager::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spSceneManager::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spSceneManager>();
        manager.RegisterClone(*this, *result);
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    bool spSceneManager::vfunc_14(spBaseObject& target, spCloneManager& manager) const
    {
        return target.IsKindOf(ClassID) && spBaseObject::vfunc_14(target, manager);
    }
    bool spSceneManager::RegisterSceneForAnalysis(SceneForAnalysis& scene)
    {
        if (!scene.systemRoot || scenes_.size() >= 4096 ||
            std::find(scenes_.begin(), scenes_.end(), &scene) != scenes_.end()) return false;
        scenes_.push_back(&scene); return true;
    }
    bool spSceneManager::UnregisterSceneForAnalysis(SceneForAnalysis& scene) noexcept
    {
        if (updating_ && currentScene_ == &scene) return false;
        const auto found = std::find(scenes_.begin(), scenes_.end(), &scene);
        if (found == scenes_.end()) return false;
        scenes_.erase(found); return true;
    }
    std::size_t spSceneManager::GetSceneCountForAnalysis() const noexcept { return scenes_.size(); }
    spSceneManager::SceneForAnalysis* spSceneManager::GetCurrentSceneForAnalysis() const noexcept { return currentScene_; }
    const std::list<spSceneManager::SceneForAnalysis*>& spSceneManager::GetScenesForAnalysis() const noexcept { return scenes_; }
    bool spSceneManager::UpdateWorldForAnalysis(const spNode::Matrix3* camera) noexcept
    {
        if (updating_) return false;
        updating_ = true; bool result = true; std::size_t visits = 0;
        for (auto it = scenes_.begin(); it != scenes_.end(); ++it)
        {
            if (++visits > 4096) { result = false; break; }
            currentScene_ = *it;
            if (!currentScene_->systemRoot || !currentScene_->systemRoot->UpdateWorldForAnalysis(0, camera)) result = false;
        }
        currentScene_ = nullptr; updating_ = false; return result;
    }
}
