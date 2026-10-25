#include "spPS2PipelineManager.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        spPS2PipelineManager* PipelineSingleton = nullptr;
        std::unique_ptr<spBaseObject> CreatePipelineManager()
        {
            return std::make_unique<spPS2PipelineManager>();
        }
        const spRTTIRecord Record{spPS2PipelineManager::ClassID,
            spBaseObject::ClassID, "spPS2PipelineManager", &spBaseObject::StaticRTTI(),
            &CreatePipelineManager, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }

    spPS2PipelineManager::spPS2PipelineManager()
    {
        // Native factory2061D0 constructs the empty map and writes gp-455C.
        PipelineSingleton = this;
    }
    spPS2PipelineManager::~spPS2PipelineManager()
    {
        factories_.clear();
        // 206044 clears unconditionally; it does not restore an older instance.
        PipelineSingleton = nullptr;
    }
    const spRTTIRecord& spPS2PipelineManager::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spPS2PipelineManager::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spPS2PipelineManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2PipelineManager>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // Native2060C0 calls inherited100320; registry is not copied.
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    spPS2PipelineManager* spPS2PipelineManager::SingletonForAnalysis() noexcept
    {
        return PipelineSingleton;
    }
    bool spPS2PipelineManager::RegisterFactoryForAnalysis(
        std::int32_t key, FactoryBindingForAnalysis binding)
    {
        factories_[key] = binding;
        return true;
    }
    bool spPS2PipelineManager::FindFactoryForAnalysis(
        std::int32_t key, FactoryBindingForAnalysis& output) const noexcept
    {
        const auto found = factories_.find(key);
        if (found == factories_.end()) return false;
        output = found->second;
        return true;
    }
    spPS2PipelineManager::DispatchForAnalysis
        spPS2PipelineManager::CreatePipelineForAnalysis(std::int32_t key) const
    {
        FactoryBindingForAnalysis binding;
        if (!FindFactoryForAnalysis(key, binding) || !binding.create) return {};
        // Capture before calling: a callback may replace its own registry entry.
        return {true, binding.create(binding.context)};
    }
    std::size_t spPS2PipelineManager::FactoryCountForAnalysis() const noexcept
    {
        return factories_.size();
    }

    PS2Key0SelectionForAnalysis SelectPS2Key0PipelineForAnalysis(
        const PS2Key0DescriptorForAnalysis& descriptor,
        const std::uint32_t* table, std::size_t tableCount) noexcept
    {
        PS2Key0SelectionForAnalysis result;
        if (!table || descriptor.field18 >= tableCount) return result;
        result.tableIndexInRange = true;
        result.programAddress = table[descriptor.field18];
        result.missingProgramRequiresDiagnostic = result.programAddress == 0;
        const std::uint32_t divisor = descriptor.field20 + std::uint32_t{6};
        if (divisor == 0) return result;
        const std::uint32_t numerator =
            ((std::uint32_t{1024} - (descriptor.field3C ? 256U : 0U) - 260U) >> 1) - 20U;
        const std::uint32_t quotient = numerator / divisor;
        // R5900 MULT writes low32 to v1 (rd=3); only low32 are later stored.
        result.field0C = quotient * descriptor.field20
            + std::uint32_t{2} * (std::uint32_t{3} * (quotient + 3U) + 1U);
        result.capacityDefined = true;
        return result;
    }
}
