#pragma once

// Inferred game-source location.  The class identity is exact on PC and PS2,
// but neither executable preserves an original wxEngineCore source/header
// path.  The ForAnalysis seam is intentionally not presented as native API.

#include "Code/Sparkplug/spEngineCore.h"

#include <cstddef>
#include <memory>

namespace winx::reconstruction
{
    class wxEngineCore final : public sparkplug::reconstruction::spEngineCore
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x34B85918;

        using AnalysisAction = void (*)(void* context);

        struct AnalysisActionBinding final
        {
            AnalysisAction action = nullptr;
            void* context = nullptr;
        };

        wxEngineCore() noexcept = default;
        ~wxEngineCore() override;

        wxEngineCore(const wxEngineCore&) = delete;
        wxEngineCore& operator=(const wxEngineCore&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(
                sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        // The only class-local override runs Winx managers first, delegates to
        // the matching spEngineCore frame boundary, then restores temporary
        // renderer state.  The concrete manager classes are not reconstructed
        // yet, so an explicit action sequence represents only that ordering.
        [[nodiscard]] bool RunFrameBoundaryForAnalysis(
            const AnalysisActionBinding* actions,
            std::size_t actionCount) const;
    };
}
