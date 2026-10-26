#include "spMessageQueue.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <memory>

namespace sparkplug::reconstruction
{
    spMessageQueue::spMessageQueue(spTaskTimer& source, spMessageQueueHost* host) noexcept
        : source_(source), host_(host)
    {
    }

    spMessageQueue::~spMessageQueue()
    {
        ClearForAnalysis();
    }

    const spRTTIRecord& spMessageQueue::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "spMessageQueue",
            &spBaseObject::StaticRTTI(), nullptr, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }

    const spRTTIRecord& spMessageQueue::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    void spMessageQueue::EnqueueForAnalysis(spNotificationForAnalysis& packet,
        std::uint32_t delay, spMessageQueueReceiverBindingForAnalysis binding)
    {
        const auto due = source_.GetStateForAnalysis().currentMilliseconds + delay;
        packet.word04 = due;
        auto* node = storage_.Allocate();
        node->notification = packet;
        node->binding = binding;
        if (!head_ || due < head_->notification.word04)
        {
            node->next = head_;
            head_ = node;
            return;
        }
        auto* previous = head_;
        while (previous->next && due >= previous->next->notification.word04)
            previous = previous->next;
        node->next = previous->next;
        previous->next = node;
    }

    spMessageQueueTickResultForAnalysis spMessageQueue::TickForAnalysis()
    {
        const auto now = source_.GetStateForAnalysis().currentMilliseconds;
        while (head_ && head_->notification.word04 <= now)
        {
            const auto key = head_->notification.word0C;
            if (key == 0 && (head_->notification.word14 == 0 || !head_->binding.receiver
                || head_->binding.token != head_->notification.word14))
                return spMessageQueueTickResultForAnalysis::UnqualifiedReceiverBinding;
            if (key != 0 && !host_ && !spSubscriptionManager::GetInstance())
                return spMessageQueueTickResultForAnalysis::UnqualifiedManagerProvider;

            // This ownership guard belongs to our storage replacement. The
            // original keeps the unlinked pool node alive until after Notify.
            auto release = [this](Node* node) { storage_.Release(node); };
            std::unique_ptr<Node, decltype(release)> current(head_, release);
            head_ = current->next;
            if (key != 0)
            {
                auto* manager = spSubscriptionManager::GetInstance();
                if (!manager)
                {
                    manager = &host_->CreateSubscriptionManagerForAnalysis();
                    if (spSubscriptionManager::GetInstance() != manager)
                        return spMessageQueueTickResultForAnalysis::UnqualifiedManagerProvider;
                }
                (void)manager->DispatchForAnalysis(key, &current->notification);
            }
            else
                current->binding.receiver->vfunc_0C(&current->notification);
            // current releases here, before re-reading the possibly changed
            // head. Do not cache next across a callback or nested Tick.
        }
        return spMessageQueueTickResultForAnalysis::Completed;
    }

    void spMessageQueue::ClearForAnalysis() noexcept
    {
        while (head_)
        {
            auto* next = head_->next;
            storage_.Release(head_);
            head_ = next;
        }
    }

    std::size_t spMessageQueue::PendingCountForAnalysis() const noexcept
    {
        std::size_t count = 0;
        for (auto* node = head_; node; node = node->next)
            ++count;
        return count;
    }

    std::size_t spMessageQueue::LiveMessageCountForAnalysis() const noexcept
    {
        return storage_.LiveCount();
    }

    std::vector<spNotificationForAnalysis> spMessageQueue::PendingMessagesForAnalysis() const
    {
        std::vector<spNotificationForAnalysis> result;
        for (auto* node = head_; node; node = node->next)
            result.push_back(node->notification);
        return result;
    }
}
