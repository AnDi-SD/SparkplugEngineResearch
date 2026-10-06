#pragma once

// Our boundary for the original secondary input interface. No device polling,
// keyboard mapping, COM call or host input state is implemented here.
#include <cstdint>
#include <functional>

namespace sparkplug::analysis::host
{
    struct spInputDeviceQueriesForAnalysis final
    {
        std::function<bool(std::uint32_t)> slot1;
        std::function<bool(std::uint32_t)> slot2;
        std::function<std::uint32_t(std::uint32_t)> slot3;
        std::function<std::int32_t(std::uint32_t)> slot4;
        std::function<void(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t)> slot5;
        std::function<float(std::uint32_t)> slot6;
    };

    class spDXInputDeviceReferencesForAnalysis
    {
    public:
        virtual ~spDXInputDeviceReferencesForAnalysis() = default;
        // Original source is the lazily created input manager's +0x28 cell.
        [[nodiscard]] virtual std::uintptr_t GetInputInterface() noexcept = 0;
        virtual void AddReference(std::uintptr_t object) noexcept = 0;
        virtual void ReleaseReference(std::uintptr_t object) noexcept = 0;
        virtual void Unacquire(std::uintptr_t device) noexcept = 0;
    };
}
