#pragma once

// Inferred header/module path.  The executable proves the class name and its
// proximity to SparkplugPC classes, but retains no direct source-path string.

#include "../SparkBase/spErrorManager.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spPCErrorManager final : public spErrorManager
    {
    public:
        static constexpr spClassID ClassID = 0x12162D8E;

        struct Presentation final
        {
            const char* title = nullptr;
            std::uint32_t messageBoxFlags = 0;
            std::uint32_t consoleCode = 0;
        };

        spPCErrorManager();
        ~spPCErrorManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] AnalysisHandlerBinding
            ResolveHandlerForAnalysis() const noexcept override;

        // Exact severity mapping from PC 0x004C34B0.  The modal MessageBoxA
        // side effect is intentionally not reproduced by the portable seam.
        [[nodiscard]] static Presentation ClassifyForAnalysis(
            spErrorSeverity severity) noexcept;

    private:
        static void NonModalHandlerForAnalysis(
            const AnalysisDispatch& dispatch,
            void* context) noexcept;
    };
}
