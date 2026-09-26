#pragma once
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace winx::reconstruction
{
    // Our explicit bridge to unreconstructed application services. Numeric IDs
    // identify original PC globals/functions/virtual byte offsets, not host addresses.
    // Handles and pointer-valued fields are pointer-sized on the host.
    class wxAppHelperHost
    {
    public:
        using Handle = std::uintptr_t;
        virtual ~wxAppHelperHost() = default;
        virtual Handle ResolveForAnalysis(std::uint32_t service, bool create) = 0;
        virtual void ClearServiceForAnalysis(std::uint32_t service) = 0;
        virtual Handle InvokeForAnalysis(std::uint32_t operation, Handle receiver,
            std::initializer_list<Handle> arguments = {}) = 0;
        virtual Handle InvokeVirtualForAnalysis(std::uint32_t slot, Handle receiver,
            std::initializer_list<Handle> arguments = {}) = 0;
        virtual Handle InvokeTextForAnalysis(std::uint32_t operation, Handle receiver,
            std::string_view text, std::initializer_list<Handle> arguments = {}) = 0;
        virtual Handle ReadHandleForAnalysis(Handle, std::uint32_t offset) = 0;
        virtual std::uint32_t ReadWordForAnalysis(Handle, std::uint32_t offset) = 0;
        virtual std::uint8_t ReadByteForAnalysis(Handle, std::uint32_t offset) = 0;
        virtual void WriteWordForAnalysis(Handle, std::uint32_t offset, std::uint32_t) = 0;
        virtual void WriteByteForAnalysis(Handle, std::uint32_t offset, std::uint8_t) = 0;
        virtual Handle InteriorForAnalysis(Handle, std::uint32_t offset) = 0;
        // Includes native stream open/seek/read/close/delete; absence means open failed.
        virtual std::optional<std::string> ReadConfigurationForAnalysis(std::string_view name) = 0;
        // Descriptor table70ACD4 has918 rows. Names returned here are owned strings;
        // the native temporary name allocation/free is absorbed by this boundary.
        virtual std::uint32_t GetAssetKindForAnalysis(std::uint32_t index) = 0;
        virtual std::string GetAssetNameForAnalysis(std::uint32_t index) = 0;
        virtual std::string ResolveAssetPathForAnalysis(std::string_view name,
            std::uint32_t category, std::uint32_t group, std::uint32_t capacity) = 0;
        virtual bool CheckFileForAnalysis(std::string_view path) = 0;
        virtual void ReportMissingFileForAnalysis(std::string_view path) = 0;
        virtual void ReportStateMemoryForAnalysis(std::int32_t state, double megabytes) = 0;
    };
}
