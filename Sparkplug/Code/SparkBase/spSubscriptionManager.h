#pragma once

// Inferred header path.  The class name and behavior are executable-backed;
// original public method names and the concrete container typedef are absent.

#include "spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>

namespace sparkplug::reconstruction
{
    class spSubscriptionManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0xE4567D00;

        spSubscriptionManager() noexcept;
        ~spSubscriptionManager() override;

        spSubscriptionManager(const spSubscriptionManager&) = delete;
        spSubscriptionManager& operator=(const spSubscriptionManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spSubscriptionManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Analytical names for PS2 0x0010E3B0, 0x0010E210 and 0x0010DE90
        // (PC 0x00416150, 0x004163A0 and 0x00415A20). Native callers pass an integer key and
        // an spBaseObject whose notification virtual is invoked on dispatch.
        [[nodiscard]] bool SubscribeForAnalysis(
            std::uint32_t key,
            spBaseObject& subscriber);
        [[nodiscard]] bool UnsubscribeForAnalysis(
            std::uint32_t key,
            spBaseObject& subscriber);
        [[nodiscard]] std::size_t DispatchForAnalysis(
            std::uint32_t key,
            const void* notification);

        [[nodiscard]] std::size_t GetGroupCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetSubscriberCountForAnalysis(
            std::uint32_t key) const noexcept;

    private:
        static spSubscriptionManager* instance_;
        std::map<std::uint32_t, std::set<spBaseObject*>> subscriptions_;
    };
}
