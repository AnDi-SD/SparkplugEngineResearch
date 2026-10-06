#include "spDXMouse.h"
#include <cstring>
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMouse()
        {
            auto* boundary = spDXMouse::FactoryContextForAnalysis::CurrentBoundaryForAnalysis();
            if (!boundary) throw std::logic_error("Mouse RTTI factory host context is not supplied");
            return spDXMouse::CreateForAnalysis(*boundary);
        }
        // Original factory004CC4F0 is nonnull. Its external dependency is bound
        // through our explicit borrowed context, separate from native globals.
        const spRTTIRecord MouseRecord{spDXMouse::ClassID, spDXInputDevice::ClassID,
            "spDXMouse", &spDXInputDevice::StaticRTTI(), &CreateMouse, nullptr};
        const bool MouseRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(MouseRecord);
        std::int32_t Signed(std::uint32_t bits) noexcept
        { std::int32_t value; std::memcpy(&value, &bits, sizeof(value)); return value; }
        bool Reject(std::string* error, const char* message)
        { if (error) *error = message; return false; }
    }
    spDXMouse::spDXMouse(BoundaryForAnalysis& boundary)
        : spDXInputDevice(boundary), boundary_(boundary), window_(0),
          visible94_(1), exclusive95_(0), cursorPosition_(PointForAnalysis{0,0}) {}
    std::unique_ptr<spDXMouse> spDXMouse::CreateForAnalysis(BoundaryForAnalysis& boundary)
    { return std::make_unique<spDXMouse>(boundary); }
    const spRTTIRecord& spDXMouse::StaticRTTI() noexcept
    { (void)MouseRegistered; return MouseRecord; }
    const spRTTIRecord& spDXMouse::vfunc_18() const noexcept { return MouseRecord; }
    std::unique_ptr<spBaseObject> spDXMouse::vfunc_10(spCloneManager& manager) const
    {
        auto destination = cloneFactory_ ? cloneFactory_() : CreateForAnalysis(boundary_);
        if (!destination) return {};
        manager.RegisterCloneForAnalysis(*this, *destination);
        if (!vfunc_14(*destination, manager)) return {};
        return destination;
    }
    void spDXMouse::SetRuntimeForAnalysis(std::uintptr_t window, const CacheForAnalysis& cache)
    {
        window_ = window; position_ = cache.position;
        packets_ = {cache.relativeAxes,cache.buttons,cache.changedAxes,cache.changedButtons};
        SyncCacheForAnalysis();
    }
    void spDXMouse::SyncCacheForAnalysis() noexcept
    {
        if (position_) cache_ = CacheForAnalysis{packets_.relativeAxes,packets_.buttons,
            packets_.changedAxes,packets_.changedButtons,*position_};
        else cache_.reset();
    }
    std::optional<bool> spDXMouse::StartupForAnalysis(std::uintptr_t window,
        StartupBoundaryForAnalysis& startup, std::string* error)
    {
        const auto rectangle = boundary_.GetClientRectangle(startup.GetApplicationWindow());
        // The x86 subtract wraps before signed division toward zero.
        position_ = std::array<std::uint32_t,2>{static_cast<std::uint32_t>(Signed(
            static_cast<std::uint32_t>(rectangle.right) - static_cast<std::uint32_t>(rectangle.left)) / 2),
            static_cast<std::uint32_t>(Signed(
            static_cast<std::uint32_t>(rectangle.bottom) - static_cast<std::uint32_t>(rectangle.top)) / 2)};
        SyncCacheForAnalysis();
        auto& state = MutableStateForAnalysis();
        state.field48 = 0x727ea0;
        startup.CreateMouseDevice(state.inputInterface4C, 0x727ea0, state.device50, 0);
        if (!state.device50)
        { Reject(error, "Original startup dereferences a missing CreateDevice output; outside host domain"); return std::nullopt; }
        capabilities_[0] = 44;
        startup.GetMouseCapabilities(state.device50, capabilities_);
        std::array<std::optional<std::uint8_t>,128> configuration{};
        startup.ReadMouseExclusiveConfiguration(configuration);
        constexpr std::uint8_t expected[5]{'t','r','u','e',0};
        bool equal = true;
        for (std::size_t i = 0; i < 5; ++i)
        {
            if (!configuration[i])
            { Reject(error, "Original mouse configuration comparison reads an unspecified output byte"); return std::nullopt; }
            if (*configuration[i] != expected[i]) { equal = false; break; }
        }
        exclusive95_ = equal ? 1 : 0;
        state.field80 = 1;
        window_ = window;
        if (!state.device50)
        { Reject(error, "Original startup dereferences a missing device after GetCapabilities/configuration"); return std::nullopt; }
        startup.SetMouseDataFormat(state.device50, 0x712b6c);
        if (!state.device50)
        { Reject(error, "Original startup dereferences a missing device after SetDataFormat"); return std::nullopt; }
        startup.SetMouseCooperativeLevel(state.device50, window, equal ? 5u : 6u);
        if (!state.device50)
        { Reject(error, "Original startup dereferences a missing device after SetCooperativeLevel"); return std::nullopt; }
        constexpr sparkplug::analysis::host::spDXMouseBufferPropertyForAnalysis property{20,16,0,0,1024};
        startup.SetMouseBufferProperty(state.device50, 1, property);
        if (!state.device50)
        { Reject(error, "Original startup dereferences a missing device after SetProperty"); return std::nullopt; }
        boundary_.Acquire(state.device50);
        state.field44 = 1;
        return true;
    }
    bool spDXMouse::SetCursorVisibleForAnalysis(std::uint8_t visible,
        CursorBoundaryForAnalysis& cursor, std::string* error)
    {
        if (!visible94_) return Reject(error, "Established original cursor visibility byte is not supplied");
        if (*visible94_ == visible) return true;
        if (visible)
        {
            if (!cursorPosition_) return Reject(error, "Established original saved cursor position is not supplied");
            cursor.ReleaseMouseCapture();
            cursor.SetMouseCursorPosition(cursorPosition_->x, cursorPosition_->y);
            cursor.ShowMouseCursor(1);
        }
        else
        {
            if (!window_) return Reject(error, "Established original mouse window is not supplied");
            cursor.SetMouseCapture(*window_);
            cursorPosition_ = cursor.GetMouseCursorPosition();
            cursor.ShowMouseCursor(0);
        }
        visible94_ = visible;
        return true;
    }
    void spDXMouse::ClearPacketCachesForAnalysis() noexcept
    {
        packets_.relativeAxes.fill(0); packets_.buttons.fill(0);
        packets_.changedAxes.fill(0); packets_.changedButtons.fill(0);
        SyncCacheForAnalysis();
    }
    bool spDXMouse::ProcessEventsForAnalysis(const EventForAnalysis* events, std::size_t count, std::string* error)
    {
        if (!position_ || !window_) return Reject(error, "Established original mouse position/window is not supplied");
        if (count > events_.size() || (count && !events))
            return Reject(error, "Original mouse event-buffer domain exceeded");
        auto& cache = packets_;
        cache.relativeAxes.fill(0); cache.changedAxes.fill(0); cache.changedButtons.fill(0);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& event = events[i];
            if (event.offset == 0 || event.offset == 4 || event.offset == 8)
                cache.relativeAxes[event.offset / 4] += event.data;
            else if (event.offset >= 12 && event.offset <= 19)
            {
                const auto index = event.offset - 12;
                cache.buttons[index] = (event.data & 0x80u) ? 1 : 0;
                cache.changedButtons[index] = 1;
            }
        }
        (*position_)[0] += cache.relativeAxes[0]; (*position_)[1] += cache.relativeAxes[1];
        SyncCacheForAnalysis();
        const auto rectangle = boundary_.GetClientRectangle(*window_);
        const std::int32_t upper[2]{rectangle.right, rectangle.bottom};
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            // Original compares right/bottom directly and handles the upper
            // bound before zero. Empty/negative bounds can therefore yield -1.
            if (Signed((*position_)[axis]) >= upper[axis])
                (*position_)[axis] = static_cast<std::uint32_t>(upper[axis]) - 1u;
            else if (Signed((*position_)[axis]) < 0) (*position_)[axis] = 0;
        }
        SyncCacheForAnalysis();
        return true;
    }
    std::optional<bool> spDXMouse::PollForAnalysis(std::string* error)
    {
        if (!GetStateForAnalysis().device50) return true; // original early return
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            std::uint32_t count = 1024;
            const auto result = boundary_.GetDeviceData(GetStateForAnalysis().device50, 20, events_, count, 0);
            if (result == 0x8007000cu || result == 0x8007001eu)
            {
                ClearPacketCachesForAnalysis();
                boundary_.Acquire(GetStateForAnalysis().device50); // original re-read after callback
            }
            else if (result == 1)
            { ClearPacketCachesForAnalysis(); return true; }
            else if (result == 0)
            {
                auto state = GetStateForAnalysis(); state.acquired45 = 1; SetStateForAnalysis(state);
                if (!ProcessEventsForAnalysis(events_.data(), count, error)) return std::nullopt;
                return true;
            }
        }
        boundary_.ReportNotAcquired();
        auto state = GetStateForAnalysis(); state.acquired45 = 0; SetStateForAnalysis(state);
        return false;
    }
    std::optional<bool> spDXMouse::PhysicalSlot1ForAnalysis(std::uint32_t code) const
    {
        if (!GetStateForAnalysis().acquired45 || code < 100 || code > 107) return false;
        return packets_.buttons[code - 100] == 1;
    }
    std::optional<bool> spDXMouse::PhysicalSlot2ForAnalysis(std::uint32_t code) const
    {
        if (!GetStateForAnalysis().acquired45 || code < 100 || code > 107) return false;
        return packets_.changedButtons[code - 100] == 1;
    }
    std::optional<std::uint32_t> spDXMouse::PhysicalSlot3ForAnalysis(std::uint32_t code) const
    {
        if (!GetStateForAnalysis().acquired45 || code < 108 || code > 109) return 0u;
        if (!position_) return std::nullopt;
        return (*position_)[code - 108];
    }
    std::optional<std::int32_t> spDXMouse::PhysicalSlot4ForAnalysis(std::uint32_t code) const
    {
        if (!GetStateForAnalysis().acquired45 || code < 108 || code > 110) return 0;
        return Signed(packets_.relativeAxes[code - 108]);
    }
}
