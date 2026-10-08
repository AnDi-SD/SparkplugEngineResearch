#pragma once
// Original PS2 class/factory/state flow. File and analytical API names are
// inferred. Own native tail is unwritten until supplied/initialized/polled.
#include "spPS2InputDevice.h"
#include "../../Analysis/Host/spPS2MouseHost.h"

namespace sparkplug::reconstruction
{
    class spPS2Mouse : public spPS2InputDevice
    {
    public:
        static constexpr spClassID ClassID = 0x5E6722F3;
        using BoundaryForAnalysis = sparkplug::analysis::host::spPS2MouseBoundaryForAnalysis;
        using TailForAnalysis = std::array<std::optional<std::uint8_t>, 0x98>;
        explicit spPS2Mouse(BoundaryForAnalysis* boundary = nullptr) noexcept : boundary_(boundary) {}
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        void SetBoundaryForAnalysis(BoundaryForAnalysis* boundary) noexcept { boundary_ = boundary; }
        [[nodiscard]] const TailForAnalysis& GetTailForAnalysis() const noexcept { return tail_; }
        [[nodiscard]] TailForAnalysis& MutableTailForAnalysis() noexcept { return tail_; }
        [[nodiscard]] std::optional<std::uint32_t> ReadTailWordForAnalysis(std::uint32_t offset) const noexcept;
        void WriteTailWordForAnalysis(std::uint32_t offset, std::uint32_t value);
        [[nodiscard]] std::optional<bool> InitializeForAnalysis(std::string* error = nullptr);
        [[nodiscard]] std::optional<bool> PollForAnalysis(std::string* error = nullptr);
        // Original callback1F1F80 reads the global semaphore again when called.
        [[nodiscard]] bool RpcCompletionForAnalysis(std::string* error = nullptr);
        // Original1F1740 is JR RA/NOP; its source-level meaning is unknown.
        void Native1F1740ForAnalysis() const noexcept {}
        [[nodiscard]] std::optional<bool> PhysicalSlot1ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<bool> PhysicalSlot2ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<std::uint32_t> PhysicalSlot3ForAnalysis(std::uint32_t) const;
        [[nodiscard]] std::optional<std::int32_t> PhysicalSlot4ForAnalysis(std::uint32_t) const;
        void PhysicalSlot5ForAnalysis(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const noexcept {}
        [[nodiscard]] float PhysicalSlot6ForAnalysis(std::uint32_t) const noexcept { return 0.0F; }
        [[nodiscard]] QueriesForAnalysis GetQueriesForAnalysis() const;
    private:
        [[nodiscard]] std::optional<std::int32_t> InitializeRpcForAnalysis();
        [[nodiscard]] bool WaitForAnalysis();
        [[nodiscard]] bool SignalForAnalysis();
        [[nodiscard]] std::optional<std::int32_t> CallForAnalysis(unsigned function, std::uint32_t receive);
        [[nodiscard]] std::optional<bool> ProcessEventForAnalysis();
        void ClearForAnalysis(std::uint32_t offset, std::uint32_t bytes) noexcept;
        BoundaryForAnalysis* boundary_;
        TailForAnalysis tail_{};
    };
}
