#pragma once

namespace sparkplug::reconstruction
{
    // A borrowed data reader, not an FX initialization callback. The native
    // projection declaration is not reconstructed by this interface. One
    // stable logical identity represents the native owner word18.
    class spProjectionFxHost
    {
    public:
        virtual ~spProjectionFxHost() = default;
        virtual const void* ReadProjectionMaterial20ForAnalysis(const void* projection) const noexcept = 0;
    };
}
