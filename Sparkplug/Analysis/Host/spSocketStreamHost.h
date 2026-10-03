#pragma once
// Our dependency boundary, not recovered game code. No implicit network success.
#include "../../Code/Sparkplug/spNetwork.h"

namespace sparkplug::reconstruction
{
    class spSocketStreamHost
    {
    public:
        virtual ~spSocketStreamHost()=default;
        virtual std::unique_ptr<spNetwork> CreateNetwork()=0;
        // Native invalid-network path creates error(type=0,level=1,code=9),
        // records spSocketStream.cpp line 51, adds the message and submits it.
        virtual void ReportInvalidNetwork()=0;
    };
    void SetSocketStreamFactoryHostForAnalysis(std::shared_ptr<spSocketStreamHost>);
    [[nodiscard]] std::shared_ptr<spSocketStreamHost> GetSocketStreamFactoryHostForAnalysis();
}
