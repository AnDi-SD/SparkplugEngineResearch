#pragma once

// Inferred header/TU path.  The class name proves the DirectInput boundary,
// but no original source-path string survives in the protected PC image.

#include "../Sparkplug/spInputManager.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spDXInputManager final : public spInputManager
    {
    public:
        static constexpr spClassID ClassID = 0x10F20027;
        static constexpr std::size_t ControllerCapacity = 4;

        spDXInputManager() noexcept = default;
        ~spDXInputManager() override;

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
