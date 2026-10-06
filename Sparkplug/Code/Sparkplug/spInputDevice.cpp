#include "spInputDevice.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord InputDeviceRecord{spInputDevice::ClassID, spCrossPlatform::ClassID,
            "spInputDevice", &spCrossPlatform::StaticRTTI(), nullptr, nullptr};
        const bool InputDeviceRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(InputDeviceRecord);
    }
    const spRTTIRecord& spInputDevice::StaticRTTI() noexcept
    { (void)InputDeviceRegistered; return InputDeviceRecord; }
    const spRTTIRecord& spInputDevice::vfunc_18() const noexcept { return InputDeviceRecord; }
    void spInputDevice::AppendBindingForAnalysis(std::uint32_t physical, std::uint32_t logical)
    { bindings_.push_back({physical, logical}); }
    void spInputDevice::ClearBindingsForAnalysis() noexcept { bindings_.clear(); }
    const std::vector<spInputDevice::BindingForAnalysis>& spInputDevice::GetBindingsForAnalysis() const noexcept
    { return bindings_; }

    std::optional<std::size_t> spInputDevice::CollectForAnalysis(
        std::uint32_t logical, bool hasProvider, std::string* error)
    {
        std::size_t count = 0;
        for (const auto& binding : bindings_) count += binding.logicalCode == logical;
        // These two checks are our admission guards. The native routine writes
        // beyond scratch for oversized maps and invokes an unresolved interface
        // for a base-only object. Neither behavior gets a fabricated fallback.
        if (count > ScratchCapacityForAnalysis || (count && !hasProvider))
        {
            if (error) *error = count > ScratchCapacityForAnalysis
                ? "Original input scratch overflow is outside the reconstructed domain"
                : "Original input interface callback is not supplied";
            return std::nullopt;
        }
        std::size_t index = 0;
        for (const auto& binding : bindings_)
            if (binding.logicalCode == logical) scratch_[index++] = binding.physicalCode;
        return count;
    }
    std::optional<bool> spInputDevice::QuerySlot1ForAnalysis(
        std::uint32_t logical, const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot1), error);
        if (!count) return std::nullopt;
        for (std::size_t i = 0; i < *count; ++i)
            if (queries.slot1(*scratch_[i])) return true;
        return false;
    }
    std::optional<bool> spInputDevice::QuerySlot2ForAnalysis(
        std::uint32_t logical, const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot2), error);
        if (!count) return std::nullopt;
        for (std::size_t i = 0; i < *count; ++i)
            if (queries.slot2(*scratch_[i])) return true;
        return false;
    }
    std::optional<std::uint32_t> spInputDevice::QuerySlot3ForAnalysis(
        std::uint32_t logical, const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot3), error);
        if (!count) return std::nullopt;
        return *count ? queries.slot3(*scratch_[0]) : 0u;
    }
    std::optional<std::int32_t> spInputDevice::QuerySlot4ForAnalysis(
        std::uint32_t logical, const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot4), error);
        if (!count) return std::nullopt;
        return *count ? queries.slot4(*scratch_[0]) : 0;
    }
    bool spInputDevice::CommandSlot5ForAnalysis(std::uint32_t logical,
        std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
        const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot5), error);
        if (!count) return false;
        if (*count) queries.slot5(*scratch_[0], a1, a2, a3);
        return true;
    }
    std::optional<float> spInputDevice::QuerySlot6ForAnalysis(
        std::uint32_t logical, const QueriesForAnalysis& queries, std::string* error)
    {
        const auto count = CollectForAnalysis(logical, bool(queries.slot6), error);
        if (!count) return std::nullopt;
        return *count ? queries.slot6(*scratch_[0]) : 0.0f;
    }
}
