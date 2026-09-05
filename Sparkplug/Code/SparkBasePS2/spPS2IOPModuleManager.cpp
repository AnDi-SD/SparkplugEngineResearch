#include "spPS2IOPModuleManager.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        constexpr const char* DefaultModuleRoot =
            "host0:c:/usr/local/sce/iop/modules/";

        std::unique_ptr<spBaseObject> CreatePS2IOPModuleManager()
        {
            return std::make_unique<spPS2IOPModuleManager>();
        }

        const spRTTIRecord PS2IOPModuleManagerRecord{
            spPS2IOPModuleManager::ClassID,
            spBaseObject::ClassID,
            "spPS2IOPModuleManager",
            &spBaseObject::StaticRTTI(),
            &CreatePS2IOPModuleManager,
            nullptr,
        };
    }

    spPS2IOPModuleManager* spPS2IOPModuleManager::instance_ = nullptr;

    spPS2IOPModuleManager::spPS2IOPModuleManager()
        : moduleRoot_(DefaultModuleRoot)
    {
        instance_ = this;
    }

    spPS2IOPModuleManager::~spPS2IOPModuleManager()
    {
        instance_ = nullptr;
    }

    const spRTTIRecord& spPS2IOPModuleManager::StaticRTTI() noexcept
    {
        return PS2IOPModuleManagerRecord;
    }

    spPS2IOPModuleManager* spPS2IOPModuleManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spPS2IOPModuleManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2IOPModuleManager>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    bool spPS2IOPModuleManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native vtable reuses the empty spBaseObject copy slot.  The root,
        // flag and loaded-module list are deliberately not cloned.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPS2IOPModuleManager::vfunc_18() const noexcept
    {
        return PS2IOPModuleManagerRecord;
    }

    bool spPS2IOPModuleManager::sub_001E9860(const char* moduleRoot)
    {
        if (moduleRoot == nullptr)
        {
            return false;
        }
        moduleRoot_ = moduleRoot;
        return true;
    }

    bool spPS2IOPModuleManager::sub_001E98C0(
        const char* moduleName,
        const bool forceReload,
        const char* moduleRootOverride)
    {
        if (moduleName == nullptr)
        {
            return false;
        }

        const auto duplicate = std::find(
            loadedModules_.begin(), loadedModules_.end(), moduleName);
        if (duplicate != loadedModules_.end() && !forceReload)
        {
            return true;
        }

        if (moduleLoader_ == nullptr)
        {
            // The native target always has the PS2 loader.  A host build must
            // not pretend that a module was loaded when no backend is bound.
            return false;
        }

        const std::string path = std::string{
            moduleRootOverride == nullptr ? moduleRoot_.c_str() : moduleRootOverride}
            + moduleName + ".IRX";

        for (std::size_t attempt = 0;
             attempt < NativeMaximumLoadAttempts;
             ++attempt)
        {
            if (moduleLoader_(path.c_str(), moduleLoaderContext_) >= 0)
            {
                // Native forced reloads append another list entry.
                loadedModules_.emplace_back(moduleName);
                return true;
            }
        }
        return false;
    }

    void spPS2IOPModuleManager::sub_001E9B20() noexcept
    {
        field18_ = true;
    }

    const std::string& spPS2IOPModuleManager::GetModuleRoot() const noexcept
    {
        return moduleRoot_;
    }

    bool spPS2IOPModuleManager::GetField18() const noexcept
    {
        return field18_;
    }

    std::size_t spPS2IOPModuleManager::GetLoadedModuleCount() const noexcept
    {
        return loadedModules_.size();
    }

    const std::string* spPS2IOPModuleManager::GetLoadedModule(
        const std::size_t index) const noexcept
    {
        return index < loadedModules_.size() ? &loadedModules_[index] : nullptr;
    }

    void spPS2IOPModuleManager::SetModuleLoaderForAnalysis(
        const ModuleLoader loader,
        void* context) noexcept
    {
        moduleLoader_ = loader;
        moduleLoaderContext_ = context;
    }
}
