#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAnimationManagerHost.h"
#include <array>
#include <map>
#include <string>
#include <string_view>

namespace winx::reconstruction
{
    class wxAnimationManager : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x9816352C;
        static constexpr std::size_t TableCount = 68;
        using Resource = void*;
        using Row = std::array<std::string, 8>; // mode,direction,action,status,jump,phase,variant,name
        struct CachedForAnalysis { Resource resource; std::uint32_t owner; };
        struct SetDescriptorForAnalysis { const char* name; std::uint32_t group; };
        struct PathDescriptorForAnalysis { std::uint32_t category, group; };
        enum class TokenForAnalysis { Mode, Direction, Action, Status, Variant };

        explicit wxAnimationManager(wxAnimationManagerHost&);
        ~wxAnimationManager() override;
        wxAnimationManager(const wxAnimationManager&) = delete;
        wxAnimationManager& operator=(const wxAnimationManager&) = delete;
        static void SetFactoryHostForAnalysis(wxAnimationManagerHost*) noexcept;
        static wxAnimationManager* GetInstance() noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Copy and notification use the inherited spBaseObject behavior.
        static std::uint32_t ParseTokenForAnalysis(TokenForAnalysis, std::string_view) noexcept;
        static std::uint32_t PackKeyForAnalysis(const Row&) noexcept;
        static SetDescriptorForAnalysis DescribeSetForAnalysis(std::uint32_t set, std::uint32_t level) noexcept;
        static PathDescriptorForAnalysis DescribePathForAnalysis(std::uint32_t set) noexcept;
        Resource GetAnimationForAnalysis(std::string_view name, std::uint32_t set);
        void LoadSetForAnalysis(std::uint32_t set);
        void ReleaseSetForAnalysis(std::uint32_t set);
        void ClearForAnalysis() noexcept;
        Resource SelectForAnalysis(std::uint32_t key, std::uint32_t table) const;
        const auto& GetTablesForAnalysis() const noexcept { return tables_; }
        const auto& GetCacheForAnalysis() const noexcept { return cache_; }
    private:
        wxAnimationManagerHost& host_;
        std::map<std::string, CachedForAnalysis, std::less<>> cache_;
        std::array<std::map<std::uint32_t, Resource>, TableCount> tables_;
    };
}
