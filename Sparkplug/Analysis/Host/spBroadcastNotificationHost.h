#pragma once

// Our synchronous application-owned creation boundary. The returned object
// must be a real reconstructed manager whose constructor publishes it, and
// its application owner must retain it beyond the notification call.
#include <functional>

namespace sparkplug::reconstruction
{
    class spSubscriptionManager;
    using spSubscriptionManagerFactoryForAnalysis =
        std::function<spSubscriptionManager&()>;
}
