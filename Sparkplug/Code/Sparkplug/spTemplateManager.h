#pragma once

// Inferred header/TU path. Native registrations expose this class name, while
// the concrete spTemplateObject header and original method names are absent.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <list>
#include <memory>
#include <string_view>

namespace sparkplug::reconstruction
{
    class spTemplateManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x04BB6643;

        spTemplateManager() noexcept;
        ~spTemplateManager() override;

        spTemplateManager(const spTemplateManager&) = delete;
        spTemplateManager& operator=(const spTemplateManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spTemplateManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // spTemplateObject is not reconstructed yet. spNamedObject retains
        // the proven lookup contract without inventing that larger class.
        [[nodiscard]] bool AddForAnalysis(std::shared_ptr<spNamedObject> instance);
        [[nodiscard]] spNamedObject* FindForAnalysis(std::string_view name) const noexcept;
        void ClearForAnalysis() noexcept;
        [[nodiscard]] std::size_t GetTemplateCountForAnalysis() const noexcept;

    private:
        static spTemplateManager* instance_;
        std::list<std::shared_ptr<spNamedObject>> templates_;
    };
}
