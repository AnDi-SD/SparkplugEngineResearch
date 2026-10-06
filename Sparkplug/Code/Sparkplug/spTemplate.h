#pragma once

// Exact original TU path: Z:\Sparkplug\Code\Sparkplug\spTemplate.cpp.
// Native PC/PS2 object graphs use intrusive links/refcounts. This portable
// component restores the descriptor-list/copy/cycle policies with host RAII.
// Native file loading, runtime attachment and manager registration stay open.
#include "spTemplateObject.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTemplate final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x6D86570A;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;

        // HOST injection/accessors around PC+14/+18/+1C descriptor links,
        // PC/PS2+20 opaque copy word and PC+38/PS2+38 resource-path string.
        // Names and validation of these facades are not original declarations.
        [[nodiscard]] bool AddObjectForAnalysis(std::shared_ptr<spTemplateObject> object);
        void ClearObjectsForAnalysis() noexcept { objects_.clear(); }
        [[nodiscard]] const auto& GetObjectsForAnalysis() const noexcept { return objects_; }
        void SetField20ForAnalysis(std::uint32_t word) noexcept { field20_ = word; }
        [[nodiscard]] std::uint32_t GetField20ForAnalysis() const noexcept { return field20_; }
        void SetResourcePathForAnalysis(std::string path) { resourcePath_ = std::move(path); }
        [[nodiscard]] const std::string& GetResourcePathForAnalysis() const noexcept { return resourcePath_; }

        // PC0059EBD0 / PS200153880. A state3 descriptor points through its
        // loaded TemplateInstance+14 owner. Equal owner or equal full resource
        // path means dependency; otherwise traverse that owner's descriptors.
        // Missing/invalid foreign state or a host recursion bound returns no
        // result. No unresolved graph is promoted to a successful copy.
        [[nodiscard]] std::optional<bool> DependsOnTemplateForAnalysis(const spTemplate& target) const noexcept;

    private:
        [[nodiscard]] std::optional<bool> DependsOnTemplateForAnalysis(
            const spTemplate& target, std::uint32_t depth) const noexcept;
        std::vector<std::shared_ptr<spTemplateObject>> objects_;
        std::uint32_t field20_ = 0;
        std::string resourcePath_;
    };
}
