#pragma once

// Original PC abstract registration and field prefix. The native primary
// thread-interface vptr precedes a CrossPlatform subobject at+4; the portable
// hierarchy unifies them and does not claim native multiple-inheritance ABI.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spThread : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x3DFE3B16;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override = 0;
        [[nodiscard]] virtual bool CreateForAnalysis(std::uint32_t entryToken, std::uint32_t bodyParameter) = 0;
        [[nodiscard]] virtual bool WaitForAnalysis(std::uint32_t milliseconds) = 0;
        [[nodiscard]] virtual bool IsRunningForAnalysis() = 0;
        [[nodiscard]] virtual bool ResumeForAnalysis() = 0;
        [[nodiscard]] virtual bool SuspendForAnalysis() = 0;
        virtual void SleepForAnalysis(std::uint32_t milliseconds) = 0;
        [[nodiscard]] virtual bool TerminateForAnalysis(std::uint32_t exitCode) = 0;

    protected:
        // Full native object+18/+1C, not offsets into its secondary subobject.
        std::uint32_t bodyParameter_ = 0;
        std::uint8_t opaqueByte_ = 0;
    };
}
