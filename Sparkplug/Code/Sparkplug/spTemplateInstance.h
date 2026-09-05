#pragma once

// Inferred header path. Both shipped binaries prove the class identity and
// default "Instance Root" object; relationship algorithms remain evidence-only.

#include "spNode.h"

#include <cstddef>
#include <list>
#include <memory>

namespace sparkplug::reconstruction
{
    class spTemplateInstance final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x1F6A7DA5;

        spTemplateInstance();
        ~spTemplateInstance() override;

        spTemplateInstance(const spTemplateInstance&) = delete;
        spTemplateInstance& operator=(const spTemplateInstance&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable inspection of constructor-proven state.
        [[nodiscard]] const spNode& GetInstanceRootForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetAttachedObjectCountForAnalysis() const noexcept;

    private:
        const spBaseObject* field14_ = nullptr;
        std::list<std::shared_ptr<spBaseObject>> attachedObjects_;
        std::unique_ptr<spNode> instanceRoot_;
    };
}
