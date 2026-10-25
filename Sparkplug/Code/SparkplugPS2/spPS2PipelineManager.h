#pragma once

// Inferred declaration path. Names ending ForAnalysis are portable interfaces,
// not recovered native signatures or the PS2 STL object layout.
#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace sparkplug::reconstruction
{
    class spPS2PipelineManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x2E7FAA26;
        static constexpr std::size_t NativeSizeForAnalysis = 0x24;
        static constexpr std::size_t NativeRegistryOffsetForAnalysis = 0x14;

        // Known callbacks ignore incoming arguments and return a pipeline pointer;
        // the original callback declaration is unknown. Context and uintptr_t
        // are explicit host transport, not its PS2 ABI.
        // Bindings borrow context; the caller owns returned pipeline lifetime.
        using FactoryForAnalysis = std::uintptr_t (*)(void* context);
        struct FactoryBindingForAnalysis final
        {
            FactoryForAnalysis create = nullptr;
            void* context = nullptr;
        };
        struct DispatchForAnalysis final
        {
            bool completed = false;
            std::uintptr_t pipeline = 0;
        };

        spPS2PipelineManager();
        ~spPS2PipelineManager() override;
        spPS2PipelineManager(const spPS2PipelineManager&) = delete;
        spPS2PipelineManager& operator=(const spPS2PipelineManager&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] static spPS2PipelineManager* SingletonForAnalysis() noexcept;

        // 205F50/206270: signed32 key, insertion or replacement, returns1.
        // A NULL callback is stored too; original registration does not check it.
        bool RegisterFactoryForAnalysis(std::int32_t key, FactoryBindingForAnalysis binding);
        [[nodiscard]] bool FindFactoryForAnalysis(
            std::int32_t key, FactoryBindingForAnalysis& output) const noexcept;
        // 205EC0 calls the found callback on EVERY lookup, with no instance cache.
        // Miss and NULL callback are analytical incomplete results: the native
        // miss falls through to JALR with an unassigned s0, not a safe NULL result.
        [[nodiscard]] DispatchForAnalysis CreatePipelineForAnalysis(std::int32_t key) const;
        [[nodiscard]] std::size_t FactoryCountForAnalysis() const noexcept;

    private:
        std::map<std::int32_t, FactoryBindingForAnalysis> factories_;
    };

    // Adjacent key0 pipeline function2074D0, NOT a method of the native manager.
    // Only fields read by that function are captured; this is not object layout.
    struct PS2Key0DescriptorForAnalysis final
    {
        std::uint32_t field18 = 0;
        std::uint32_t field20 = 0;
        std::uint32_t field3C = 0;
    };
    struct PS2Key0SelectionForAnalysis final
    {
        bool tableIndexInRange = false;
        bool missingProgramRequiresDiagnostic = false;
        bool capacityDefined = false;
        std::uint32_t programAddress = 0;
        std::uint32_t field0C = 0;
    };
    // Caller supplies the proven table extent. Native has no index bound before
    // its load; rejecting an out-of-range host span is an analytical boundary.
    // A zero slot enters diagnostics, then computes capacity IF that call returns.
    // Native division-by-zero behavior is deliberately not substituted by host /.
    [[nodiscard]] PS2Key0SelectionForAnalysis SelectPS2Key0PipelineForAnalysis(
        const PS2Key0DescriptorForAnalysis& descriptor,
        const std::uint32_t* table, std::size_t tableCount) noexcept;
}
