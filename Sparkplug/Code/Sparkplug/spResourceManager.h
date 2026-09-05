#pragma once

// Inferred declaration path. The class name is present in both shipped
// executables, but no original header or translation-unit path was recovered.
// Methods ending in ForAnalysis preserve observed behavior without claiming
// the lost source spellings.

#include "spResource.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spResourceManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0xA4B9923B;

        // Exact values selected by PC 0x004584B0 / PS2 resource-manager
        // classification paths. The native enum type/name is not recovered.
        enum class ResourceKindForAnalysis : std::uint32_t
        {
            Texture = 1,
            Mesh = 2,
            Unsupported = 0x10,
        };

        spResourceManager() noexcept;
        ~spResourceManager() override;

        spResourceManager(const spResourceManager&) = delete;
        spResourceManager& operator=(const spResourceManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spResourceManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] static ResourceKindForAnalysis ClassifyForAnalysis(
            spClassID classID) noexcept;

        // Native manager caches only named spTexture/spMesh descendants.
        // Entries are non-owning: spResource destruction unregisters itself.
        [[nodiscard]] bool RegisterForAnalysis(spResource& resource);
        [[nodiscard]] bool UnregisterForAnalysis(spResource& resource) noexcept;
        [[nodiscard]] spResource* FindForAnalysis(
            spClassID requestedClassID,
            const char* name) const noexcept;

        // PS2 0x0017D360 stores the flag/count and reserves the cache only
        // when the flag is true. Failure is explicit at this safe host seam.
        [[nodiscard]] bool ConfigureReserveForAnalysis(
            bool reserveEnabled,
            std::uint32_t resourceCount) noexcept;

        [[nodiscard]] std::size_t GetResourceCountForAnalysis() const noexcept;
        [[nodiscard]] bool IsReserveEnabledForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetReserveCountForAnalysis() const noexcept;
        [[nodiscard]] std::int32_t GetField1CForAnalysis() const noexcept;

    private:
        struct Entry final
        {
            ResourceKindForAnalysis kind = ResourceKindForAnalysis::Unsupported;
            spResource* resource = nullptr;
        };

        static spResourceManager* instance_;
        bool reserveEnabled_ = false;
        std::uint32_t reserveCount_ = 0;
        std::int32_t field1C_ = -1;
        std::vector<Entry> resources_;
    };
}
