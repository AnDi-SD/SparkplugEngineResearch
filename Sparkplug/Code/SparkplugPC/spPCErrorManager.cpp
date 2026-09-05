#include "spPCErrorManager.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCErrorManager()
        {
            return std::make_unique<spPCErrorManager>();
        }

        const spRTTIRecord PCErrorManagerRecord{
            spPCErrorManager::ClassID,
            spErrorManager::ClassID,
            "spPCErrorManager",
            &spErrorManager::StaticRTTI(),
            &CreatePCErrorManager,
            nullptr,
        };
    }

    spPCErrorManager::spPCErrorManager()
        : spErrorManager(PcDataCapacity)
    {
    }

    const spRTTIRecord& spPCErrorManager::StaticRTTI() noexcept
    {
        return PCErrorManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPCErrorManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCErrorManager>();
        manager.RegisterClone(*this, *clone);
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spPCErrorManager::vfunc_18() const noexcept
    {
        return PCErrorManagerRecord;
    }

    spErrorManager::AnalysisHandlerBinding
        spPCErrorManager::ResolveHandlerForAnalysis() const noexcept
    {
        // Native provider 0x004C3580 returns 0x004C34B0.
        return {&NonModalHandlerForAnalysis, nullptr};
    }

    spPCErrorManager::Presentation spPCErrorManager::ClassifyForAnalysis(
        const spErrorSeverity severity) noexcept
    {
        switch (severity)
        {
        case spErrorSeverity::Information:
            return {"Information", 0x40, 0x0A};
        case spErrorSeverity::Warning:
            return {"Warning", 0x30, 0x0E};
        case spErrorSeverity::Fatal:
            return {"Fatal error", 0x10, 0x04};
        case spErrorSeverity::Error:
        default:
            return {"Error", 0x10, 0x04};
        }
    }

    void spPCErrorManager::NonModalHandlerForAnalysis(
        const AnalysisDispatch& dispatch,
        void*) noexcept
    {
        // Native 0x004C34B0 shows a modal MessageBoxA and routes the same text
        // to 0x00413500.  Automated reconstruction must remain non-modal, so
        // preserve a debugger-visible diagnostic without blocking the host.
#if defined(_WIN32)
        if (dispatch.text != nullptr)
        {
            ::OutputDebugStringA(dispatch.text);
            ::OutputDebugStringA("\n");
        }
#else
        (void)dispatch;
#endif
    }
}
