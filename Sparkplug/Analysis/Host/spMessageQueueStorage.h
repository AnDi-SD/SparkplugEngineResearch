#pragma once

// Our portable replacement for the native descriptor pool. These allocations
// are not the original four64-entry blocks, their address/reuse policy, or ABI.
// Queue owns every node; packet pointers and receiver bindings stay borrowed.
#include "spMessageQueueHost.h"
#include "Code/SparkBase/spNotification.h"
#include <cassert>
#include <cstddef>

namespace sparkplug::reconstruction
{
    class spMessageQueueStorageForAnalysis final
    {
    public:
        struct Node final
        {
            spNotificationForAnalysis notification;
            spMessageQueueReceiverBindingForAnalysis binding;
            Node* next = nullptr;
        };

        spMessageQueueStorageForAnalysis() noexcept = default;
        spMessageQueueStorageForAnalysis(const spMessageQueueStorageForAnalysis&) = delete;
        spMessageQueueStorageForAnalysis& operator=(const spMessageQueueStorageForAnalysis&) = delete;

        [[nodiscard]] Node* Allocate()
        {
            auto* node = new Node;
            ++live_;
            return node;
        }
        void Release(Node* node) noexcept
        {
            assert(node && live_);
            delete node;
            --live_;
        }
        [[nodiscard]] std::size_t LiveCount() const noexcept { return live_; }

    private:
        std::size_t live_ = 0;
    };
}
