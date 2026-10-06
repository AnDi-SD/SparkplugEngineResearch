#pragma once

// Own original PC policy, with explicitly supplied platform callbacks. The
// portable class does not expose a Win32 or original pointer/vtable ABI.
#include "../Sparkplug/spThread.h"
#include "../../Analysis/Host/spPCThreadHost.h"

namespace sparkplug::reconstruction
{
    class spPCThread final : public spThread
    {
    public:
        static constexpr spClassID ClassID = 0x438758EA;
        using HostForAnalysis = host::spPCThreadHost;
        using HandleForAnalysis = HostForAnalysis::Handle;
        struct StateForAnalysis final
        {
            std::uint32_t bodyParameter = 0;
            std::uint8_t opaqueByte = 0;
            HandleForAnalysis handle = 0;
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Borrowed provider must outlive any operation; clone is unbound.
        void BindHostForAnalysis(HostForAnalysis& host) noexcept { host_ = &host; }
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const noexcept;
        // Literal native-state fixture/restored handle, not native Create.
        void SetStateForAnalysis(StateForAnalysis state) noexcept;
        [[nodiscard]] bool CreateForAnalysis(std::uint32_t entryToken, std::uint32_t bodyParameter) override;
        [[nodiscard]] bool WaitForAnalysis(std::uint32_t milliseconds) override;
        [[nodiscard]] bool IsRunningForAnalysis() override;
        [[nodiscard]] bool ResumeForAnalysis() override;
        [[nodiscard]] bool SuspendForAnalysis() override;
        void SleepForAnalysis(std::uint32_t milliseconds) override;
        [[nodiscard]] bool TerminateForAnalysis(std::uint32_t exitCode) override;
        // Original destructor does not wait, terminate or CloseHandle; this
        // class likewise does not own OS handle cleanup. Provider/caller own it.

    private:
        HostForAnalysis* host_ = nullptr;
        HandleForAnalysis handle_ = 0;
    };
}
