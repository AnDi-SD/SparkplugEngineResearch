#pragma once

// Inferred header/TU path.  The class name, IDs and behavior are native;
// original member and method spellings are unavailable.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spDebugManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x37054B40;
        static constexpr std::size_t FlagCount = 12;
        static constexpr std::uint32_t CycleLength = 20;

        spDebugManager() noexcept;
        ~spDebugManager() override;

        spDebugManager(const spDebugManager&) = delete;
        spDebugManager& operator=(const spDebugManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spDebugManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool SetFlagForAnalysis(
            std::size_t index, bool enabled) noexcept;
        [[nodiscard]] bool GetFlagForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] std::uint32_t AdvanceCycleIndexForAnalysis() noexcept;
        [[nodiscard]] std::uint32_t GetCycleIndexForAnalysis() const noexcept;

    private:
        static spDebugManager* instance_;
        std::uint32_t cycleIndex_ = 0;
        std::array<std::uint8_t, FlagCount> flags_{};
    };
}
