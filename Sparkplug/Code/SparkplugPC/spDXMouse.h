#pragma once
// Registered original PC class. Portable state/API names are analytical and
// The original input-manager/COM dependency is supplied by our host boundary.
#include "spDXInputDevice.h"
#include "../../Analysis/Host/spDXMouseHost.h"
#include <functional>

namespace sparkplug::reconstruction
{
    class spDXMouse : public spDXInputDevice
    {
    public:
        static constexpr spClassID ClassID = 0x72F650C7;
        using BoundaryForAnalysis = sparkplug::analysis::host::spDXMouseBoundaryForAnalysis;
        using EventForAnalysis = sparkplug::analysis::host::spDXMouseEventForAnalysis;
        using EventsForAnalysis = sparkplug::analysis::host::spDXMouseEventsForAnalysis;
        using StartupBoundaryForAnalysis = sparkplug::analysis::host::spDXMouseStartupBoundaryForAnalysis;
        using CursorBoundaryForAnalysis = sparkplug::analysis::host::spDXMouseCursorBoundaryForAnalysis;
        using CapabilitiesForAnalysis = sparkplug::analysis::host::spDXMouseCapabilitiesForAnalysis;
        using PointForAnalysis = sparkplug::analysis::host::spDXMousePointForAnalysis;
        using FactoryContextForAnalysis = sparkplug::analysis::host::spDXMouseFactoryContextForAnalysis;
        struct PacketsForAnalysis final
        {
            std::array<std::uint32_t, 3> relativeAxes;
            std::array<std::uint8_t, 8> buttons;
            std::array<std::uint32_t, 3> changedAxes;
            std::array<std::uint8_t, 8> changedButtons;
        };
        struct CacheForAnalysis final
        {
            std::array<std::uint32_t, 3> relativeAxes;
            std::array<std::uint8_t, 8> buttons;
            std::array<std::uint32_t, 3> changedAxes;
            std::array<std::uint8_t, 8> changedButtons;
            std::array<std::uint32_t, 2> position;
        };
        explicit spDXMouse(BoundaryForAnalysis&);
        [[nodiscard]] static std::unique_ptr<spDXMouse> CreateForAnalysis(BoundaryForAnalysis&);
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Optional host allocation override. Without it Clone uses the
        // recovered constructor with the same established input dependency.
        using CloneFactoryForAnalysis = std::function<std::unique_ptr<spDXMouse>()>;
        void SetCloneFactoryForAnalysis(CloneFactoryForAnalysis factory) { cloneFactory_ = std::move(factory); }

        // Supplies established absolute X/Y along with optional runtime packet
        // overrides. The original ctor zeros packets/window but leaves X/Y.
        void SetRuntimeForAnalysis(std::uintptr_t window, const CacheForAnalysis&);
        // Original 004CC5C0 uses the application's window for initial centering
        // and its own parameter for device cooperation and subsequent polling.
        [[nodiscard]] std::optional<bool> StartupForAnalysis(std::uintptr_t window,
            StartupBoundaryForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] const CapabilitiesForAnalysis& GetCapabilitiesForAnalysis() const noexcept { return capabilities_; }
        [[nodiscard]] std::optional<std::uint8_t> GetExclusiveFlagForAnalysis() const noexcept { return exclusive95_; }
        void SetExclusiveFlagForAnalysis(std::uint8_t flag) noexcept { exclusive95_ = flag; }
        void SetCursorRuntimeForAnalysis(std::uint8_t visible, PointForAnalysis position) noexcept
        { visible94_ = visible; cursorPosition_ = position; }
        [[nodiscard]] std::optional<std::uint8_t> GetCursorVisibleFlagForAnalysis() const noexcept { return visible94_; }
        [[nodiscard]] const std::optional<PointForAnalysis>& GetCursorPositionForAnalysis() const noexcept { return cursorPosition_; }
        [[nodiscard]] bool SetCursorVisibleForAnalysis(std::uint8_t visible,
            CursorBoundaryForAnalysis&, std::string* error = nullptr);
        [[nodiscard]] const std::optional<CacheForAnalysis>& GetCacheForAnalysis() const noexcept { return cache_; }
        [[nodiscard]] const PacketsForAnalysis& GetPacketsForAnalysis() const noexcept { return packets_; }
        [[nodiscard]] const std::optional<std::array<std::uint32_t,2>>& GetPositionForAnalysis() const noexcept { return position_; }
        [[nodiscard]] const EventsForAnalysis& GetEventsForAnalysis() const noexcept { return events_; }
        [[nodiscard]] std::optional<bool> PollForAnalysis(std::string* error = nullptr);
        [[nodiscard]] bool ProcessEventsForAnalysis(const EventForAnalysis*, std::size_t count,
            std::string* error = nullptr);
        [[nodiscard]] std::optional<bool> PhysicalSlot1ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<bool> PhysicalSlot2ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<std::uint32_t> PhysicalSlot3ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<std::int32_t> PhysicalSlot4ForAnalysis(std::uint32_t) const;
        void PhysicalSlot5ForAnalysis(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const noexcept {}
        [[nodiscard]] float PhysicalSlot6ForAnalysis(std::uint32_t) const noexcept { return 0.0f; }

        // Our borrowed provider connects these original physical slots to the
        // common logical-input dispatch. This object must outlive the callbacks.
        // An active slot3 reading unspecified absolute X/Y throws logic_error;
        // the original zero for a gated or out-of-range query is preserved.
        [[nodiscard]] QueriesForAnalysis GetQueriesForAnalysis() const;

    private:
        void ClearPacketCachesForAnalysis() noexcept;
        void SyncCacheForAnalysis() noexcept;
        BoundaryForAnalysis& boundary_;
        CloneFactoryForAnalysis cloneFactory_;
        std::optional<std::uintptr_t> window_;
        std::optional<CacheForAnalysis> cache_;
        PacketsForAnalysis packets_{};
        std::optional<std::array<std::uint32_t,2>> position_;
        CapabilitiesForAnalysis capabilities_{};
        std::optional<std::uint8_t> visible94_, exclusive95_;
        std::optional<PointForAnalysis> cursorPosition_;
        EventsForAnalysis events_{}; // buffer bytes outside returned count are analytical storage only
    };
}
