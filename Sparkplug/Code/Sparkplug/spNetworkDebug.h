#pragma once
// Native class/behavior; inferred path and analytical API names. PC only.
#include "../SparkBase/spBaseObject.h"
#include "../../Analysis/Host/spNetworkDebugHost.h"
#include <vector>

namespace sparkplug::reconstruction
{
    class spNetworkDebug : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x178ABAB6;
        struct StateForAnalysis
        {
            std::uint8_t consoleEnabled=0,fileEnabled=0,unknown12=0,subscribed=0;
            bool hasStream=false,hasLock=false;
            std::string lastLine;
            std::uint8_t timerActive=1;
            std::uint32_t accumulated=0,start=0;
            std::uintptr_t context=0;
            std::int32_t level=1;
        };
        struct PacketForAnalysis
        {
            std::uint16_t source,destination,type;
            std::vector<std::uint8_t> data;
        };
        explicit spNetworkDebug(std::shared_ptr<spNetworkDebugHost> host=GetNetworkDebugFactoryHostForAnalysis());
        ~spNetworkDebug() override;
        spNetworkDebug(const spNetworkDebug&)=delete;
        spNetworkDebug& operator=(const spNetworkDebug&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Original inherits the root successful no-op copy, even for self-copy.
        void vfunc_0C(const void* notification) noexcept override;
        virtual bool StartForAnalysis();
        virtual void StopForAnalysis() noexcept;
        void SetFileOutputForAnalysis(std::uint8_t enabled,const std::string& suffix);
        void RenderFrameRateForAnalysis();
        void LogForAnalysis(const char* category,std::int32_t level,const char* format,...);
        // Post-CRT analytical entry; preserves all owned assembly/output logic.
        void LogTextForAnalysis(const std::string& category,std::int32_t level,const std::string& text);
        void DumpPacketForAnalysis(const PacketForAnalysis& packet);
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const;
        // Literal external field setup, not a claim to recovered setter names.
        void SetControlsForAnalysis(std::uint8_t console,std::uint8_t unknown12,
            std::uint8_t subscribed,std::uintptr_t context,std::int32_t level) noexcept;
    private:
        bool CanLog(std::int32_t level) const noexcept;
        void EmitLine(const std::string& category,const std::string& text);
        void StopTimer() noexcept;
        std::shared_ptr<spNetworkDebugHost> host_;
        StateForAnalysis state_;
        std::unique_ptr<spNetworkDebugStreamForAnalysis> stream_;
        std::uintptr_t lock_=0;
    };
}
