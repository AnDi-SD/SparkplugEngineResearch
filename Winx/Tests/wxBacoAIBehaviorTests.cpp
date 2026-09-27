#include "Analysis/PC/wxBacoAIBehaviorAbi.h"
#include "Analysis/PS2/wxBacoAIBehaviorAbi.h"
#include "Code/wxAIAction.h"
#include "Code/wxBacoAIBehavior.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(bool value, const char* message)
    {
        if (!value)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    class TraceAction final : public wxAIAction
    {
    public:
        TraceAction(std::string& trace, const char* name) : trace_(trace), name_(name) {}
        void vfunc_34_ClearForAnalysis() noexcept override
        {
            trace_ += std::string("exit-") + name_ + ';';
            wxAIAction::vfunc_34_ClearForAnalysis();
        }
    private:
        std::string& trace_;
        const char* name_;
    };

    class TraceBridge final : public wxBaseAIBehaviorActionBridge
    {
    public:
        explicit TraceBridge(std::string& trace) : trace_(trace) {}
        void EnterForAnalysis(wxAIAction& action, std::uint32_t parameter) noexcept override
        {
            trace_ += (&action == first_ ? "enter-first:" : "enter-zero:");
            trace_ += std::to_string(parameter) + ';';
            action.vfunc_30();
        }
        wxAIAction* first_ = nullptr;
    private:
        std::string& trace_;
    };
}

int main()
{
    wxBacoAIBehavior baco;
    Require(baco.IsKindOf(wxBaseAIBehavior::ClassID)
        && baco.IsKindOf(0x796A1869)
        && baco.vfunc_18().classID == wxBacoAIBehavior::ClassID,
        "Baco RTTI chain");
    const auto& state = baco.GetConstructionStateForAnalysis();
    Require(state.flag0 == 1 && state.flag1 == 1
        && state.value0 == 500 && state.value1 == 1500
        && state.flag2 == 0 && state.flag3 == 0,
        "Baco PC/PS2 constructor stores");
    Require(baco.GetActionCountForAnalysis() == 0
        && baco.GetCurrentActionForAnalysis() == nullptr
        && baco.GetActionGateForAnalysis() == 0
        && baco.GetGateChangeEnabledForAnalysis()
        && baco.GetBaseValuesForAnalysis()[0] == 125.0f
        && baco.GetBaseValuesForAnalysis()[1] == 400.0f
        && baco.GetBaseValuesForAnalysis()[2] == 700.0f,
        "base constructor state");
    Require(baco.SelectActionForAnalysis(9, 3)
        && baco.GetCurrentActionForAnalysis() == nullptr,
        "accepted selection can leave current null");
    baco.SelectDefaultActionForAnalysis(9);
    Require(baco.GetCurrentActionForAnalysis() == nullptr,
        "Baco default action can also be absent");
    auto clone = baco.Clone();
    auto* bacoClone = dynamic_cast<wxBacoAIBehavior*>(clone.get());
    Require(bacoClone && bacoClone != &baco
        && bacoClone->GetConstructionStateForAnalysis().value1 == 1500,
        "default clone lifecycle");
    auto created = spRTTIManager::Instance().Create(wxBacoAIBehavior::ClassID);
    Require(dynamic_cast<wxBacoAIBehavior*>(created.get()) != nullptr,
        "registered Baco factory");

    std::string trace;
    TraceAction zero(trace, "zero"), first(trace, "first");
    TraceBridge bridge(trace);
    bridge.first_ = &first;
    baco.SetActionBridgeForAnalysis(&bridge);
    baco.BindActionForAnalysis(0, zero, 0);
    baco.BindActionForAnalysis(7, first, 7);
    baco.BindActionForAnalysis(0x80000000u, first, 7);
    baco.BindActionForAnalysis(0xFFFFFFFFu, nullptr, 0);
    Require(baco.FindActionForAnalysis(0x80000000u) == &first
        && baco.FindActionForAnalysis(1) == nullptr
        && baco.FindActionForAnalysis(0xFFFFFFFFu) == nullptr,
        "unsigned map lookup");
    Require(baco.SelectActionForAnalysis(0x80000000u, 0x12345678u)
        && trace == "enter-first:305419896;",
        "enter receives original parameter");
    first.SetClearableWordForAnalysis(41);
    trace.clear();
    Require(baco.SelectActionForAnalysis(0x80000000u, 9)
        && trace == "exit-first;enter-first:9;"
        && first.GetClearableWordForAnalysis() == 0,
        "same action exits and reenters");
    trace.clear();
    Require(baco.SelectActionForAnalysis(3, 5)
        && baco.GetCurrentActionForAnalysis() == &zero
        && trace == "exit-first;enter-zero:5;",
        "missing key falls back to zero");
    trace.clear();
    Require(baco.SelectActionForAnalysis(0xFFFFFFFFu, 6)
        && trace == "exit-zero;enter-zero:6;",
        "null map value falls back to zero");
    trace.clear();
    baco.SetActionGateForAnalysis(0x123402);
    Require(baco.GetActionGateForAnalysis() == 2
        && baco.GetSavedActionKeyForAnalysis() == 0
        && trace == "exit-zero;enter-zero:0;",
        "gate uses low byte and switches to zero");
    trace.clear();
    Require(!baco.SelectActionForAnalysis(0x80000000u, 1) && trace.empty(),
        "gate rejects nonzero key");
    baco.SetActionGateForAnalysis(0);
    Require(baco.GetCurrentActionForAnalysis() == &zero,
        "gate restores saved key");
    trace.clear();
    baco.SelectDefaultActionForAnalysis(0x80000000u);
    Require(baco.GetCurrentActionForAnalysis() == &zero
        && trace == "exit-zero;enter-zero:2147483648;",
        "Baco slot selects key zero and forwards its argument");
    Require(baco.SelectActionForAnalysis(7, 1), "select saved-key action");
    trace.clear();
    baco.SetActionGateForAnalysis(1);
    Require(baco.GetSavedActionKeyForAnalysis() == 7
        && baco.GetCurrentActionForAnalysis() == &zero
        && trace == "exit-first;enter-zero:0;",
        "gate saves the action's own key");
    trace.clear();
    baco.SetActionGateForAnalysis(0);
    Require(baco.GetCurrentActionForAnalysis() == &first
        && trace == "exit-zero;enter-first:0;",
        "gate restores saved key");
    baco.SetGateControlsForAnalysis(true, true);
    trace.clear();
    baco.SetActionGateForAnalysis(1);
    Require(baco.GetActionGateForAnalysis() == 1
        && baco.GetCurrentActionForAnalysis() == &first
        && trace.empty(),
        "inhibited switch retains current after gate byte changes");
    baco.SelectDefaultActionForAnalysis(99);
    Require(baco.GetCurrentActionForAnalysis() == &zero
        && trace == "exit-first;enter-zero:99;",
        "Baco default slot bypasses gate and forwards parameter");
    baco.SetGateControlsForAnalysis(false, false);
    baco.SetActionGateForAnalysis(0);
    Require(baco.GetActionGateForAnalysis() == 1,
        "disabled gate change is ignored");
    Require(baco.Clone() == nullptr, "nonempty map clone remains an open boundary");

    std::cout << "wxBacoAIBehavior reconstruction tests passed\n";
}
