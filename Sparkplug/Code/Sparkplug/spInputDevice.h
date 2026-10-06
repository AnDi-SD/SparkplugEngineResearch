#pragma once

// Class identity and algorithm are native-backed. File/API names and portable
// containers are analytical; the native layouts are recorded separately.
#include "../SparkBase/spBaseObject.h"
#include "../../Analysis/Host/spInputDeviceHost.h"
#include <array>
#include <optional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spInputDevice : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x4C615DE6;
        static constexpr std::size_t ScratchCapacityForAnalysis = 5;
        struct BindingForAnalysis final
        {
            std::uint32_t physicalCode;
            std::uint32_t logicalCode;
        };
        using QueriesForAnalysis = sparkplug::analysis::host::spInputDeviceQueriesForAnalysis;

        spInputDevice() = default;
        ~spInputDevice() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // Clone remains the inherited null result; the native factory is null.

        void AppendBindingForAnalysis(std::uint32_t physical, std::uint32_t logical);
        void ClearBindingsForAnalysis() noexcept;
        [[nodiscard]] const std::vector<BindingForAnalysis>& GetBindingsForAnalysis() const noexcept;
        [[nodiscard]] const std::array<std::optional<std::uint32_t>, ScratchCapacityForAnalysis>&
            GetScratchForAnalysis() const noexcept { return scratch_; }

        [[nodiscard]] std::optional<bool> QuerySlot1ForAnalysis(std::uint32_t logical,
            const QueriesForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] std::optional<bool> QuerySlot2ForAnalysis(std::uint32_t logical,
            const QueriesForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] std::optional<std::uint32_t> QuerySlot3ForAnalysis(std::uint32_t logical,
            const QueriesForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] std::optional<std::int32_t> QuerySlot4ForAnalysis(std::uint32_t logical,
            const QueriesForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] bool CommandSlot5ForAnalysis(std::uint32_t logical,
            std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3,
            const QueriesForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] std::optional<float> QuerySlot6ForAnalysis(std::uint32_t logical,
            const QueriesForAnalysis&, std::string* error = nullptr);

    private:
        [[nodiscard]] std::optional<std::size_t> CollectForAnalysis(
            std::uint32_t logical, bool hasProvider, std::string* error);
        std::vector<BindingForAnalysis> bindings_;
        // Native ctor leaves scratch words untouched. A disengaged cell records
        // that condition without reading indeterminate portable C++ storage.
        std::array<std::optional<std::uint32_t>, ScratchCapacityForAnalysis> scratch_{};
    };
}
