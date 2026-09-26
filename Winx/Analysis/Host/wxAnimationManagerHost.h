#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace winx::reconstruction
{
    // External stream/resource operations, not a substitute animation database.
    // Resource identity and ownership are preserved across these opaque handles.
    class wxAnimationManagerHost
    {
    public:
        virtual ~wxAnimationManagerHost() = default;
        virtual std::uint32_t GetCurrentLevelForAnalysis() = 0;
        virtual std::string ResolveAnimationPathForAnalysis(std::string_view name,
            std::uint32_t category, std::uint32_t group, std::size_t capacity) = 0;
        virtual void* LoadAnimationForAnalysis(std::string_view path) = 0;
        virtual void DestroyAnimationForAnalysis(void*) noexcept = 0;
        virtual void* OpenTableForAnalysis(std::string_view name, std::uint32_t category,
            std::uint32_t group, std::uint32_t flags) = 0;
        // Original creates TempAnimationMap, copies source, seeks (1,0).
        virtual void* CreateTokenStreamForAnalysis(void* source) = 0;
        virtual std::string ReadTokenForAnalysis(void* stream, std::size_t capacity, char delimiter) = 0;
        // Close first, then deleting destructor, for this one stream.
        virtual void CloseAndDestroyStreamForAnalysis(void*) noexcept = 0;
    };
}
