#pragma once

// Our typed translation of the native process-global clock/service bindings.
// The original constructor takes no explicit arguments; these borrowed objects
// let portable callers supply real shared services instead of opaque words.
namespace sparkplug::reconstruction
{
    class spTimer;
    class spMessageQueueHost;
}
namespace winx::reconstruction
{
    struct wxGameTimerFactoryBindingsForAnalysis final
    {
        sparkplug::reconstruction::spTimer* globalClock = nullptr;
        sparkplug::reconstruction::spMessageQueueHost* queueHost = nullptr;
    };
}
