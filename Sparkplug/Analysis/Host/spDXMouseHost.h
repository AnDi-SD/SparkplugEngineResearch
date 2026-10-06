#pragma once
// Our COM/Win32 boundary; the reconstructed class never polls host OS itself.
#include "spInputDeviceHost.h"
#include <array>
#include <optional>

namespace sparkplug::analysis::host
{
    struct spDXMouseEventForAnalysis final
    {
        std::uint32_t offset, data, timestamp, sequence, applicationData;
    };
    using spDXMouseEventsForAnalysis = std::array<spDXMouseEventForAnalysis, 1024>;
    struct spDXMouseClientRectForAnalysis final
    { std::int32_t left, top, right, bottom; };
    using spDXMouseCapabilitiesForAnalysis = std::array<std::optional<std::uint32_t>, 11>;
    struct spDXMouseBufferPropertyForAnalysis final
    { std::uint32_t size, headerSize, object, how, bufferSize; };
    struct spDXMousePointForAnalysis final
    { std::uint32_t x, y; };
    // Separate startup/cursor interfaces keep unsupported dependencies explicit.
    // An HRESULT never supplies a fabricated device or output structure.
    class spDXMouseStartupBoundaryForAnalysis
    {
    public:
        virtual ~spDXMouseStartupBoundaryForAnalysis() = default;
        [[nodiscard]] virtual std::uintptr_t GetApplicationWindow() noexcept = 0;
        virtual std::uint32_t CreateMouseDevice(std::uintptr_t inputInterface,
            std::uint32_t originalGuidAddress, std::uintptr_t& device, std::uintptr_t outer) noexcept = 0;
        virtual std::uint32_t GetMouseCapabilities(std::uintptr_t device,
            spDXMouseCapabilitiesForAnalysis&) noexcept = 0;
        virtual void ReadMouseExclusiveConfiguration(std::array<std::optional<std::uint8_t>,128>&) noexcept = 0;
        virtual std::uint32_t SetMouseDataFormat(std::uintptr_t device, std::uint32_t originalFormatAddress) noexcept = 0;
        virtual std::uint32_t SetMouseCooperativeLevel(std::uintptr_t device,
            std::uintptr_t window, std::uint32_t flags) noexcept = 0;
        virtual std::uint32_t SetMouseBufferProperty(std::uintptr_t device,
            std::uintptr_t property, const spDXMouseBufferPropertyForAnalysis&) noexcept = 0;
    };
    class spDXMouseCursorBoundaryForAnalysis
    {
    public:
        virtual ~spDXMouseCursorBoundaryForAnalysis() = default;
        virtual void ReleaseMouseCapture() noexcept = 0;
        virtual void SetMouseCursorPosition(std::uint32_t x, std::uint32_t y) noexcept = 0;
        virtual void ShowMouseCursor(std::int32_t show) noexcept = 0;
        virtual void SetMouseCapture(std::uintptr_t window) noexcept = 0;
        [[nodiscard]] virtual spDXMousePointForAnalysis GetMouseCursorPosition() noexcept = 0;
    };
    class spDXMouseBoundaryForAnalysis : public spDXInputDeviceReferencesForAnalysis
    {
    public:
        // Exact original GetDeviceData arguments: device,20,event buffer,
        // in/out count initialized to1024,flags0. HRESULT bits are preserved.
        virtual std::uint32_t GetDeviceData(std::uintptr_t device, std::uint32_t recordSize,
            spDXMouseEventsForAnalysis&, std::uint32_t& count, std::uint32_t flags) noexcept = 0;
        virtual void Acquire(std::uintptr_t device) noexcept = 0;
        // Original ignores GetClientRect's Boolean return. This boundary must
        // supply the resulting rectangle, including any established failure
        // payload; it does not fabricate dimensions when the API is unresolved.
        [[nodiscard]] virtual spDXMouseClientRectForAnalysis GetClientRectangle(std::uintptr_t window) noexcept = 0;
        virtual void ReportNotAcquired() noexcept = 0;
    };
    // Our borrowed, thread-local RTTI factory context. This is not the native
    // input-manager singleton, COM ownership, or a game field. Nested scopes
    // restore the previous context; the boundary must outlive created objects.
    class spDXMouseFactoryContextForAnalysis final
    {
    public:
        explicit spDXMouseFactoryContextForAnalysis(spDXMouseBoundaryForAnalysis& boundary) noexcept
            : previous_(current_) { current_ = &boundary; }
        ~spDXMouseFactoryContextForAnalysis() { current_ = previous_; }
        spDXMouseFactoryContextForAnalysis(const spDXMouseFactoryContextForAnalysis&) = delete;
        spDXMouseFactoryContextForAnalysis& operator=(const spDXMouseFactoryContextForAnalysis&) = delete;
        [[nodiscard]] static spDXMouseBoundaryForAnalysis* CurrentBoundaryForAnalysis() noexcept { return current_; }
    private:
        inline static thread_local spDXMouseBoundaryForAnalysis* current_ = nullptr;
        spDXMouseBoundaryForAnalysis* previous_;
    };
}
