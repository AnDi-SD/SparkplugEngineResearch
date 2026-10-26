#pragma once

// Native class identity and PC logical operations. Original TU/header and API
// spellings are unknown; this portable class does not claim the68-byte ABI.
#include "spTaskTimer.h"
#include "Analysis/Host/spMessageQueueStorage.h"
#include <vector>

namespace sparkplug::reconstruction
{
    enum class spMessageQueueTickResultForAnalysis
    {
        Completed,
        UnqualifiedReceiverBinding,
        UnqualifiedManagerProvider,
    };

    class spMessageQueue final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0xC32F87A0;

        // Actual source timer and optional application provider are borrowed.
        // The native constructor can receive a null clock, whose use faults;
        // this bounded portable profile requires a real source timer.
        explicit spMessageQueue(spTaskTimer& source, spMessageQueueHost* host = nullptr) noexcept;
        ~spMessageQueue() override;
        spMessageQueue(const spMessageQueue&) = delete;
        spMessageQueue& operator=(const spMessageQueue&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // No registered factory. Native Clone is null and Copy is inherited
        // root no-op; neither operation copies the runtime queue.

        // PC413D80: caller word04 changes before storage allocation. All eight
        // logical words are copied; only the separate typed binding is ours.
        // At direct dispatch, binding.token must match nonzero packet.word14.
        void EnqueueForAnalysis(spNotificationForAnalysis& packet, std::uint32_t delay,
            spMessageQueueReceiverBindingForAnalysis binding = {});

        // PC413B40: captures current once, unlinks before notification, keeps
        // that message alive through callbacks, then releases/reloads head.
        // Direct callbacks may Clear/Enqueue/nest Tick; broadcast callbacks
        // retain the shared manager's nonmutating-dispatch qualification.
        // Refusal statuses are OUR guards, never native false/fault results.
        [[nodiscard]] spMessageQueueTickResultForAnalysis TickForAnalysis();
        void ClearForAnalysis() noexcept;

        [[nodiscard]] std::size_t PendingCountForAnalysis() const noexcept;
        // Includes unlinked messages currently being dispatched, including
        // outer calls during nested Tick. This is own storage bookkeeping.
        [[nodiscard]] std::size_t LiveMessageCountForAnalysis() const noexcept;
        [[nodiscard]] std::vector<spNotificationForAnalysis> PendingMessagesForAnalysis() const;

    private:
        using Node = spMessageQueueStorageForAnalysis::Node;
        spTaskTimer& source_;
        spMessageQueueHost* host_;
        spMessageQueueStorageForAnalysis storage_;
        Node* head_ = nullptr;
    };
}
