#pragma once

// Inferred header/TU path; no original PS2 source-path string survives.

#include "../Sparkplug/spInputManager.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spPS2InputManager final : public spInputManager
    {
    public:
        static constexpr spClassID ClassID = 0x462B48E1;
        static constexpr std::size_t ControllerCapacity = 2;

        spPS2InputManager() noexcept = default;
        ~spPS2InputManager() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        bool InitializeForAnalysis() override;
        void ShutdownForAnalysis() noexcept override;
        [[nodiscard]] std::size_t GetMaximumControllerCountForAnalysis()
            const noexcept override;
        [[nodiscard]] bool IsControllerConnectedForAnalysis(
            std::size_t index) const noexcept override;
        bool SetControllerConnectedForAnalysis(
            std::size_t index, bool connected) noexcept;

    private:
        std::array<bool, ControllerCapacity> connected_{};
    };
}
