#pragma once
#include "Code/Sparkplug/spTaskTimer.h"
#include "Code/wxTargetManager.h"
#include <cstdint>
#include <vector>

namespace winx::reconstruction
{
    enum class wxSleepingTargetPlatformForAnalysis { Unqualified, PC, PS2 };
    struct wxSleepingTargetManagerBindingForAnalysis final
    {
        wxSleepingTargetPlatformForAnalysis platform = wxSleepingTargetPlatformForAnalysis::Unqualified;
        wxTargetManagerHost* services = nullptr;
        // Application storage, never owned/released by the action. More than
        // one manager can remain alive when native singleton stores change.
        std::vector<std::unique_ptr<wxTargetManager>>* applicationOwners = nullptr;
    };
    // Our borrowed graph bindings. These expose external records/services;
    // Sleeping's own phase/stamp decisions and both stages remain in Code.
    class wxIceGargoyleSleepingAIActionHost
    {
    public:
        virtual ~wxIceGargoyleSleepingAIActionHost() = default;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(void* command, std::uint32_t offset,
            std::uint8_t value) noexcept = 0;
        virtual std::uint32_t ReadCharacterStateWordForAnalysis(void* owner) noexcept = 0;
        // Must acquire the real wxGameTimer and return its proven physical
        // spTaskTimer prefix. Caller reads its actual currentMilliseconds.
        virtual sparkplug::reconstruction::spTaskTimer& GameTimerPrefixForAnalysis() noexcept = 0;
        // Presence observation only; does not implement the cold constructor.
        virtual bool HasExistingTargetManagerForAnalysis() noexcept = 0;
        // Stable declarative configuration WITHOUT side effects. It is read
        // before the native-global observation to validate the admitted PC
        // profile; this lookup is not an extra native application callback.
        // Successful PC acquisition executes
        // the real shared TargetManager constructor in Sleeping's own source.
        // Services must satisfy TargetManager's accepted plain-Node/null-Scene,
        // stable canonical binding/reference domain. No cold clock/OS promise.
        virtual wxSleepingTargetManagerBindingForAnalysis TargetManagerBindingForAnalysis() noexcept { return {}; }
        // Borrowed owner selector entry boundary, not its successful body.
        virtual void SelectActionForAnalysis(void* owner, std::uint32_t key,
            std::uint32_t parameter) noexcept = 0;
    };
}
