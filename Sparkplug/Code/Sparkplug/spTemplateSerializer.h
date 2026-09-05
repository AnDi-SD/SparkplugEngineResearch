#pragma once

// Exact original path proven by the PC diagnostic strings:
// Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spStream;
    class spTemplateObject;

    class spTemplateSerializer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x41577707;

        spTemplateSerializer() noexcept;
        ~spTemplateSerializer() override;

        spTemplateSerializer(const spTemplateSerializer&) = delete;
        spTemplateSerializer& operator=(const spTemplateSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native serializer entry paths install these non-owning pointers at
        // +0x10/+0x14. Exact public method names have not survived.
        void BindForAnalysis(spTemplateObject* target, spStream* input) noexcept;
        [[nodiscard]] spTemplateObject* GetTargetForAnalysis() const noexcept;
        [[nodiscard]] spStream* GetInputForAnalysis() const noexcept;
        [[nodiscard]] std::int32_t GetParentIDForAnalysis() const noexcept;
        [[nodiscard]] bool HasOutputForAnalysis() const noexcept;

    private:
        spTemplateObject* target_ = nullptr;
        spStream* input_ = nullptr;
        void* parsedDocument_ = nullptr;
        std::int32_t parentID_ = -1;
        std::array<std::uint8_t, 0x40> parentName_{};
        std::shared_ptr<spBaseObject> output_;
        bool outputReady_ = false;
        std::uint32_t field1EC_ = 0;
        bool failed_ = false;
    };
}
