#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // Borrowed foreign Projection/FX fields and leaf calls. This interface
    // neither implements the manager loop nor replaces the4243D0 owner write.
    // Handles and the manager must remain alive through each synchronous call.
    class spProjectionManagerHost
    {
    public:
        virtual ~spProjectionManagerHost()=default;
        virtual void* ReadProjectionHelper94ForAnalysis(void* projection)=0;
        virtual void WriteProjectionHelperOwner18ForAnalysis(void* helper,void* projection)=0;
        virtual std::uint32_t InvokeProjectionHelper0ForAnalysis(void* helper)=0;
        virtual std::uint32_t DrawProjection38ForAnalysis(void* projection,void* camera)=0;
    };
}
