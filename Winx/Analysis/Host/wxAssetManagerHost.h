#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace winx::reconstruction
{
    class wxAssetFileStreamForAnalysis
    {
    public:
        virtual ~wxAssetFileStreamForAnalysis() = default;
        virtual bool OpenForAnalysis(std::string_view path, std::uint32_t mode) = 0;
    };

    // Registry access, application settings and file streams are platform
    // services. The catalog and path rules remain in wxAssetManager itself.
    class wxAssetManagerHost
    {
    public:
        virtual ~wxAssetManagerHost() = default;
        virtual bool IsPs2ForAnalysis() const noexcept = 0;
        virtual std::uint32_t AppLanguageForAnalysis() const noexcept = 0;
        virtual void SetAppLanguageForAnalysis(std::uint32_t) noexcept = 0;
        virtual std::optional<std::string> ReadPcRegistryValueForAnalysis(
            std::string_view key, std::string_view value) = 0;
        virtual std::unique_ptr<wxAssetFileStreamForAnalysis> CreateFileStreamForAnalysis() = 0;
        virtual void ReportOpenFailureForAnalysis(std::string_view path) noexcept = 0;
    };
}
