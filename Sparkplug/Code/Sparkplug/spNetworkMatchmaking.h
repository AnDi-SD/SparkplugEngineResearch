#pragma once
// Inferred file path/declarations and analytical operation names. PC only.
#include "../SparkBase/spBaseObject.h"
#include "../../Analysis/Host/spNetworkMatchmakingHost.h"

namespace sparkplug::reconstruction
{
    class spNetworkMatchmaking : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x18b864ac;
        using Handle=spNetworkMatchmakingHost::Handle;
        struct StateForAnalysis
        {
            bool hasSocketStream,hasMemoryStream;
            std::string text;
        };
        explicit spNetworkMatchmaking(std::shared_ptr<spNetworkMatchmakingHost> host=GetNetworkMatchmakingFactoryHostForAnalysis());
        ~spNetworkMatchmaking() override;
        spNetworkMatchmaking(const spNetworkMatchmaking&)=delete;
        spNetworkMatchmaking& operator=(const spNetworkMatchmaking&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Notification and Copy inherit the root no-op operations.
        bool CloseForAnalysis();
        void PumpForAnalysis(); // Native return register is not a bool result.
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const;
        [[nodiscard]] Handle GetSocketStreamForAnalysis() const noexcept {return socketStream_;}
        [[nodiscard]] spMemoryStream* GetMemoryStreamForAnalysis() noexcept {return memoryStream_.get();}
    private:
        std::shared_ptr<spNetworkMatchmakingHost> host_;
        std::string text_; // Native string+18, initialized empty; purpose unknown.
        std::unique_ptr<spMemoryStream> memoryStream_;
        Handle socketStream_=0;
    };
}
