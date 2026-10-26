#include "wxGameFlowNavigation.h"
#include "Code/SparkBase/spNotification.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        void* Require(void* value)
        {
            if (!value) throw std::logic_error("GameFlow navigation requires a valid borrowed handle");
            return value;
        }
    }

    std::optional<unsigned> ClassifyGameFlowDirectionPCForAnalysis(
        const float angle, const wxGameFlowDirectionBoundsForAnalysis bounds)
    {
        if (!std::isfinite(angle) || !std::isfinite(bounds.central) || !std::isfinite(bounds.helper)
            || !(bounds.central > 0) || !(bounds.helper > 0))
            throw std::domain_error("GameFlow direction requires a finite angle and positive finite bounds");
        const double value = angle;
        const double central = bounds.central, helper = bounds.helper;
        // Multiplying a float32 by the literal3 is exact in double and x87;
        // rounding the product to float32 would change two boundary branches.
        const double three = 3 * helper;
        if (value >= -central && value <= central) return 0;
        if (value <= -three || value >= three) return 1;
        if (value >= -three && value <= -helper) return 2;
        if (value >= helper && value <= three) return 3;
        return std::nullopt; // independent constants may leave a gap
    }

    void NavigateGameFlowPCForAnalysis(wxGameFlowState& object, const std::uint32_t view,
        wxGameFlowNavigationHost& host, const spTaskTimer::ClockSourceForAnalysis& clock,
        const wxGameFlowDirectionBoundsForAnalysis bounds)
    {
        auto& timer = object.TimerForAnalysis();
        if (!timer.UpdateForAnalysis(clock)) throw std::logic_error("GameFlow navigation clock is invalid");
        auto& state = object.MutableStateForAnalysis();
        auto* input = Require(object.GetBorrowedInputForAnalysis());
        const float magnitude = host.ReadNavigationMagnitudeForAnalysis(input);
        // Quiet NaN follows the original unordered comparison into release.
        if (!(magnitude > 0.5f)) { state.field38 = 1; return; }
        if (!state.field38 && state.field34 != 0
            && timer.GetStateForAnalysis().currentMilliseconds <= state.field34)
        { state.field38 = 0; return; }

        auto* window = Require(host.ResolveNavigationWindowForAnalysis(view));
        auto* initial = host.ReadInitialNavigationWidgetForAnalysis(window);
        if (!initial)
        {
            window = Require(host.ResolveNavigationWindowForAnalysis(view));
            initial = host.ReadSelectedNavigationWidgetForAnalysis(window);
        }
        const float angle = host.ReadNavigationAngleForAnalysis(Require(object.GetBorrowedInputForAnalysis()));
        const auto direction = ClassifyGameFlowDirectionPCForAnalysis(angle,bounds);
        if (!direction) { state.field38 = 0; return; }

        // Original helper's first widget argument is unused. The link pointer
        // is retained, not copied: foreign calls may edit that same buffer.
        auto select = [&](const char* link)
        {
            if (std::strcmp(link,"empty") == 0) return true;
            auto* lookupWindow = Require(host.ResolveNavigationWindowForAnalysis(view));
            auto* candidate = host.FindNavigationWidgetForAnalysis(lookupWindow,link);
            if (!candidate) return true;
            if (!host.ReadNavigationWidgetEnabledForAnalysis(candidate)) return false;
            void* receiver = host.ResolveNavigationNotificationReceiverForAnalysis();
            SendDirectedNotificationForAnalysis(host,&object,receiver,0x27D1,0x10,0);
            auto* selectionWindow = Require(host.ResolveNavigationWindowForAnalysis(view));
            host.SelectNavigationWidgetForAnalysis(selectionWindow,candidate);
            // Reload after both foreign notification and selection callbacks.
            state.field34 = timer.GetStateForAnalysis().currentMilliseconds + std::uint32_t{500};
            return true;
        };

        void* current = initial;
        std::uint32_t failed = 0;
        for (;;)
        {
            const char* link = host.ReadNavigationLinkForAnalysis(Require(current),*direction);
            if (!link) throw std::logic_error("GameFlow navigation link is null");
            if (std::strstr(link,"empty")) break;
            if (select(link)) break;
            // A failed attempt deliberately performs another lookup using the
            // same borrowed link buffer and a freshly resolved window.
            auto* nextWindow = Require(host.ResolveNavigationWindowForAnalysis(view));
            current = host.FindNavigationWidgetForAnalysis(nextWindow,link);
            ++failed;
            if (current == initial && failed >= 20) break;
            if (failed >= 4096) throw std::logic_error("GameFlow navigation host cycle guard");
        }
        state.field38 = 0;
    }
}
