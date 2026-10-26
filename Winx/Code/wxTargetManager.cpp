#include "wxTargetManager.h"
#include "wxAssetManager.h"
#include <cstring>
#include <stdexcept>
#include <typeinfo>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxTargetManagerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateTargetManager()
        {
            if (!factoryHost) throw std::logic_error("TargetManager requires explicit resource/scene bindings");
            return std::make_unique<wxTargetManager>(*factoryHost);
        }
    }
    wxTargetManager* wxTargetManager::instance_ = nullptr;
    wxTargetManager::wxTargetManager(wxTargetManagerHost& host) : host_(host)
    {
        // Native publishes before list initialization and own resource init.
        instance_ = this;
        try { InitializeResourceForAnalysis(); }
        catch (...) { instance_ = nullptr; throw; } // host safety, no native failure qualification
    }
    wxTargetManager::~wxTargetManager()
    {
        ResetDefaultLifetimeForAnalysis();
        // Remaining native40/30 releases and list free are empty after Reset
        // in this admitted default-lifetime domain. Clear is unconditional.
        instance_ = nullptr;
    }
    void wxTargetManager::SetFactoryHostForAnalysis(wxTargetManagerHost* host) noexcept { factoryHost = host; }
    wxTargetManager* wxTargetManager::GetInstanceForAnalysis() noexcept { return instance_; }
    const spRTTIRecord& wxTargetManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "wxTargetManager",
            &spBaseObject::StaticRTTI(), &CreateTargetManager, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    const spRTTIRecord& wxTargetManager::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> wxTargetManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxTargetManager>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool wxTargetManager::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    { return spBaseObject::vfunc_14(destination, manager); }

    void wxTargetManager::ReleaseNodeForAnalysis(std::shared_ptr<wxTargetNodeBindingForAnalysis>& binding) noexcept
    {
        if (!binding) return;
        binding->referenceCount8 = static_cast<std::uint16_t>(binding->referenceCount8 - 1u);
        if (!binding->referenceCount8) binding->node.reset();
        binding.reset();
    }
    void wxTargetManager::InitializeResourceForAnalysis()
    {
        auto* assets = host_.ResolveTargetAssetManagerForAnalysis();
        if (!assets) throw std::logic_error("TargetManager requires an initialized AssetManager");
        std::string path;
        if (!assets->BuildPathForAnalysis(path, 0xFF, 14, 0, "arow.smo"))
            throw std::domain_error("TargetManager requires category14, subcategory0 and capacity255");
        auto* manager = host_.ResolveTargetResourceManagerForAnalysis();
        if (!manager) throw std::logic_error("TargetManager requires a ResourceManager");
        auto loaded = host_.LoadTargetNodeForAnalysis(*manager, path.c_str());
        if (!loaded || !loaded->node || typeid(*loaded->node) != typeid(spNode)
            || loaded->node->GetSceneForAnalysis())
            throw std::domain_error("TargetManager resource slice requires a live plain Node with no Scene association");
        if (resource_ && resource_->node == loaded->node && resource_ != loaded)
            throw std::domain_error("TargetManager requires one canonical binding per Node");
        const auto currentParent = loaded->node->GetParentForAnalysis();
        const auto knownReferences = static_cast<unsigned>(currentParent != nullptr)
            + static_cast<unsigned>(resource_ == loaded);
        const auto additions = static_cast<unsigned>(resource_ != loaded)
            + static_cast<unsigned>(currentParent == nullptr);
        if (loaded->referenceCount8 < knownReferences
            || static_cast<unsigned>(loaded->referenceCount8) + additions > 0xFFFFu)
            throw std::domain_error("TargetManager reference domain excludes inconsistent counts and AddRef wrap");
        if (resource_ && resource_->node && resource_->referenceCount8 <
            1u + static_cast<unsigned>(resource_->node->GetParentForAnalysis() != nullptr))
            throw std::domain_error("TargetManager requires its manager reference to survive scene detach");
        if (resource_ != loaded)
        {
            ReleaseNodeForAnalysis(resource_);
            loaded->referenceCount8 = static_cast<std::uint16_t>(loaded->referenceCount8 + 1u);
            resource_ = std::move(loaded);
        }
        resource_->node->SetScaleForAnalysis({2.0F, 2.0F, 2.0F});
        resource_->node->MarkLocalTransformDirtyForAnalysis();
        resource_->node->SetEnabledForAnalysis(false, true);
        // Capture native30 after virtual Enable and before resolving Scene.
        // Resolver contents cannot replace the child already captured here.
        const auto binding = resource_;
        const auto child = binding->node;
        auto* root = host_.ResolveTargetSceneRootForAnalysis();
        if (!root || root->GetSceneForAnalysis()
            || (child->GetParentForAnalysis() && child->GetParentForAnalysis() != root)
            || child.get() == root)
            throw std::domain_error("TargetManager requires a stable root and detached or already attached Node");
        const bool alreadyAttached = child->GetParentForAnalysis() == root;
        if (!root->AttachChildForAnalysis(child))
            throw std::domain_error("TargetManager source graph refused AttachChild");
        if (!alreadyAttached)
            binding->referenceCount8 = static_cast<std::uint16_t>(binding->referenceCount8 + 1u);
    }
    void wxTargetManager::ResetDefaultLifetimeForAnalysis() noexcept
    {
        current_.fill(0);
        if (resource_ && resource_->node)
        {
            if (auto* parent = resource_->node->GetParentForAnalysis())
            {
                auto detached = parent->DetachChildForAnalysis(*resource_->node);
                if (detached)
                    resource_->referenceCount8 = static_cast<std::uint16_t>(resource_->referenceCount8 - 1u);
            }
            ReleaseNodeForAnalysis(resource_);
        }
    }
    void wxTargetManager::vfunc_0C(const void* notification) noexcept
    {
        if (!notification) { lastNotification_ = wxTargetNotificationResultForAnalysis::InvalidInput; return; }
        std::uint32_t code; std::memcpy(&code, notification, sizeof(code));
        if (code == 0x2712) lastNotification_ = wxTargetNotificationResultForAnalysis::UnqualifiedActivation;
        else if (code == 0x2713) lastNotification_ = wxTargetNotificationResultForAnalysis::UnqualifiedDeactivation;
        else lastNotification_ = wxTargetNotificationResultForAnalysis::Ignored;
    }
}
