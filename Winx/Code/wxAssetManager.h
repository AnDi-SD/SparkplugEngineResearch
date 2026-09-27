#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAssetManagerHost.h"
#include <optional>

namespace winx::reconstruction
{
    // Native parent: spBaseObject, plus a secondary singleton-support vtable.
    // The portable class does not imitate that multiple-inheritance ABI.
    class wxAssetManager : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x46E341B9;
        explicit wxAssetManager(wxAssetManagerHost&);
        ~wxAssetManager() override;
        wxAssetManager(const wxAssetManager&) = delete;
        wxAssetManager& operator=(const wxAssetManager&) = delete;
        static void SetFactoryHostForAnalysis(wxAssetManagerHost*) noexcept;
        static wxAssetManager* GetInstanceForAnalysis() noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;

        void SetLanguageForAnalysis(std::uint32_t mode);
        void InitializeForAnalysis();
        bool BuildPathForAnalysis(std::string& output, std::size_t capacity,
            std::uint32_t category, std::uint32_t subcategory, std::string_view filename) const;
        std::unique_ptr<wxAssetFileStreamForAnalysis> OpenFileStreamForAnalysis(
            std::uint32_t category, std::uint32_t subcategory,
            std::string_view filename, bool secondMode);
        const char* RootForAnalysis() const noexcept { return root_ ? root_->c_str() : nullptr; }
        std::uint32_t LanguageModeForAnalysis() const noexcept { return languageMode_; }
        std::string_view LanguageDirectoryForAnalysis() const noexcept { return languageDirectory_; }
        static std::string_view CategoryForAnalysis(std::size_t index) noexcept;
        static std::string_view SubcategoryForAnalysis(std::size_t index) noexcept;
    private:
        wxAssetManagerHost& host_;
        std::optional<std::string> root_;
        std::uint32_t languageMode_ = 0;
        std::string languageDirectory_;
        bool catalogReady_ = false;
    };
}
