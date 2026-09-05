#pragma once

// Inferred header and module path.  The class name and RTTI identity are
// literal PS2 executable evidence.  Unknown original method names retain their
// entry addresses.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPS2IOPModuleManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x59264170;
        static constexpr std::size_t NativeMaximumLoadAttempts = 102;

        // Analytical host seam for the PS2 module loader.  The executable
        // invokes its platform routine with path, zero and null arguments.
        using ModuleLoader = int (*)(const char* path, void* context);

        spPS2IOPModuleManager();
        ~spPS2IOPModuleManager() override;

        spPS2IOPModuleManager(const spPS2IOPModuleManager&) = delete;
        spPS2IOPModuleManager& operator=(const spPS2IOPModuleManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spPS2IOPModuleManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool sub_001E9860(const char* moduleRoot);
        [[nodiscard]] bool sub_001E98C0(
            const char* moduleName,
            bool forceReload,
            const char* moduleRootOverride = nullptr);
        void sub_001E9B20() noexcept;

        [[nodiscard]] const std::string& GetModuleRoot() const noexcept;
        [[nodiscard]] bool GetField18() const noexcept;
        [[nodiscard]] std::size_t GetLoadedModuleCount() const noexcept;
        [[nodiscard]] const std::string* GetLoadedModule(
            std::size_t index) const noexcept;

        void SetModuleLoaderForAnalysis(
            ModuleLoader loader,
            void* context = nullptr) noexcept;

    private:
        static spPS2IOPModuleManager* instance_;
        std::string moduleRoot_;
        bool field18_ = false;
        std::vector<std::string> loadedModules_;
        ModuleLoader moduleLoader_ = nullptr;
        void* moduleLoaderContext_ = nullptr;
    };
}
