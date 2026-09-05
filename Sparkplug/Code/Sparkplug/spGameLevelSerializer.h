#pragma once

// Exact original source path:
// Z:\Sparkplug\Code\Sparkplug\spGameLevelSerializer.cpp

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spGameLevel;
    class spStream;

    class spGameLevelSerializer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x72B27469;
        static constexpr std::uint32_t BinaryHeaderMagic = 0x351E46AE;

        spGameLevelSerializer() noexcept;
        ~spGameLevelSerializer() override;

        spGameLevelSerializer(const spGameLevelSerializer&) = delete;
        spGameLevelSerializer& operator=(const spGameLevelSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        void BindForAnalysis(spGameLevel* target, spStream* input) noexcept;
        [[nodiscard]] spGameLevel* GetTargetForAnalysis() const noexcept;
        [[nodiscard]] spStream* GetInputForAnalysis() const noexcept;

    private:
        spGameLevel* target_ = nullptr;
        spStream* input_ = nullptr;
        void* parsedDocument_ = nullptr;

        // Parser-populated record retained only to preserve the proven shape;
        // no public parser contract is claimed yet.
        std::array<char, 0x40> instanceName_{};
        std::array<char, 0x100> assetPath_{};
        std::array<float, 3> position_{};
        std::array<float, 4> rotation_{};
        std::array<float, 3> scale_{};
    };
}
