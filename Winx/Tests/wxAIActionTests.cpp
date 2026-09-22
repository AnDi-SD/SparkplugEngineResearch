#include "Analysis/PC/wxAIActionAbi.h"
#include "Analysis/PS2/wxAIActionAbi.h"
#include "Code/wxAIAction.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(const bool condition, const char* const message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    class TraceChild final : public wxAIAction
    {
    public:
        explicit TraceChild(std::string& trace, const char* label)
            : trace_(trace), label_(label) {}

        void vfunc_0C(const void* const notification) noexcept override
        {
            const auto* message =
                static_cast<const wxAIActionMessageForAnalysis*>(notification);
            if (!trace_.empty()) trace_ += ';';
            trace_ += std::string("forward-") + label_ + ':'
                + std::to_string(message->code);
        }

    private:
        std::string& trace_;
        const char* label_;
    };

    class TraceParent final : public wxAIAction
    {
    public:
        explicit TraceParent(std::string& trace) : trace_(trace) {}
        wxAIAction* replacement = nullptr;

        bool vfunc_2C() noexcept override
        {
            if (!trace_.empty()) trace_ += ';';
            trace_ += "self";
            if (replacement != nullptr) SetCurrentActionForAnalysis(replacement);
            return true;
        }

    private:
        std::string& trace_;
    };

    class CountingObject final : public spBaseObject
    {
    public:
        explicit CountingObject(unsigned& destroyed) : destroyed_(destroyed) {}
        ~CountingObject() override { ++destroyed_; }
    private:
        unsigned& destroyed_;
    };

    unsigned CurrentToken(const wxAIAction& parent,
        const wxAIAction* const first, const wxAIAction* const second)
    {
        if (parent.GetCurrentActionForAnalysis() == first) return 1;
        if (parent.GetCurrentActionForAnalysis() == second) return 2;
        return 0;
    }

    int Protocol()
    {
        unsigned kind, code, childPresent, replace, clearValue;
        while (std::cin >> kind >> code >> childPresent >> replace >> clearValue)
        {
            std::string trace;
            TraceParent parent(trace);
            TraceChild first(trace, "first");
            TraceChild second(trace, "second");
            if (childPresent != 0) parent.SetCurrentActionForAnalysis(&first);
            if (replace != 0) parent.replacement = &second;
            parent.SetClearableWordForAnalysis(clearValue);
            wxAIActionMessageForAnalysis message{code};
            unsigned result = 2;
            switch (kind)
            {
            case 0: parent.vfunc_0C(&message); break;
            case 1: parent.vfunc_34_ClearForAnalysis(); break;
            case 2: result = parent.vfunc_24(); break;
            case 3: result = parent.vfunc_28(); break;
            case 4: result = parent.wxAIAction::vfunc_2C(); break;
            case 5: parent.vfunc_30(); break;
            case 6: parent.vfunc_38(); break;
            case 7: result = parent.vfunc_3C(&message); break;
            case 8: result = parent.vfunc_40(&message); break;
            default: Require(false, "unknown protocol operation");
            }
            std::cout << result << ' ' << parent.GetClearableWordForAnalysis()
                << ' ' << CurrentToken(parent, &first, &second) << '|'
                << trace << '\n';
        }
        Require(std::cin.eof(), "malformed protocol input");
        return EXIT_SUCCESS;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--protocol") return Protocol();

    Require(sizeof(winx::evidence::pc::wxAIActionLayout) == 0x3A8
        && sizeof(winx::evidence::ps2::wxAIActionLayout) == 0x3AC,
        "paired native allocation sizes");
    Require(offsetof(winx::evidence::pc::wxAIActionLayout, pathFinder) == 0x2C
        && offsetof(winx::evidence::ps2::wxAIActionLayout, pathFinder) == 0x30,
        "platform-specific embedded PathFinder offsets");

    wxAIAction action;
    Require(action.IsExactly(wxAIAction::ClassID)
        && action.IsKindOf(spBaseObject::ClassID), "RTTI identity and ancestry");
    Require(spRTTIManager::Instance().Create(wxAIAction::ClassID)->IsExactly(
        wxAIAction::ClassID), "registered factory");
    Require(action.GetCurrentActionForAnalysis() == nullptr
        && action.GetOwnedActionCountForAnalysis() == 0
        && action.GetOwnerForAnalysis() == nullptr
        && action.GetField24ForAnalysis() == nullptr, "pointer/container defaults");
    Require(action.GetDurationMillisecondsForAnalysis() == 2000
        && action.GetClearableWordForAnalysis() == 0, "integer defaults");
    for (const auto word : action.GetZeroWordsForAnalysis())
        Require(word == 0, "seven zero words");
    for (const auto value : action.GetVector0ForAnalysis()) Require(value == 0.0f, "vector0 zero");
    for (const auto value : action.GetVector1ForAnalysis()) Require(value == 0.0f, "vector1 zero");
    for (const auto value : action.GetVector2ForAnalysis()) Require(value == 0.0f, "vector2 zero");
    Require(action.GetScalarBetweenVectorsForAnalysis() == 0.0f, "middle scalar zero");

    std::string trace;
    TraceParent parent(trace);
    TraceChild first(trace, "first");
    TraceChild second(trace, "second");
    parent.SetCurrentActionForAnalysis(&first);
    parent.replacement = &second;
    const wxAIActionMessageForAnalysis special{0x1C};
    parent.vfunc_0C(&special);
    Require(trace == "self;forward-second:28", "special message hook then re-read current");
    trace.clear();
    parent.replacement = nullptr;
    parent.SetCurrentActionForAnalysis(&first);
    const wxAIActionMessageForAnalysis ordinary{27};
    parent.vfunc_0C(&ordinary);
    Require(trace == "forward-first:27", "ordinary message forwards unchanged");

    Require(action.vfunc_24() && action.vfunc_28() && action.vfunc_2C(),
        "three default true hooks");
    action.vfunc_30();
    action.vfunc_38();
    Require(action.vfunc_3C(nullptr) && !action.vfunc_40(nullptr),
        "argument true/false hooks");
    action.SetClearableWordForAnalysis(0xA5A5A5A5);
    action.vfunc_34_ClearForAnalysis();
    Require(action.GetClearableWordForAnalysis() == 0, "clear hook changes one logical word");

    unsigned destroyed = 0;
    wxAIAction source;
    wxAIAction destination;
    wxAIAction nested;
    TraceChild nestedCurrent(trace, "nested");
    nested.SetCurrentActionForAnalysis(&nestedCurrent);
    nested.SetClearableWordForAnalysis(17);
    destination.SetCurrentActionForAnalysis(&nested);
    destination.AddOwnedActionForAnalysis(std::make_unique<CountingObject>(destroyed));
    destination.SetOwnerForAnalysis(reinterpret_cast<void*>(0x100));
    destination.SetField24ForAnalysis(reinterpret_cast<void*>(0x200));
    destination.SetClearableWordForAnalysis(23);
    spCloneManager manager;
    Require(source.vfunc_14(destination, manager), "base action copy succeeds");
    Require(destination.GetCurrentActionForAnalysis() == nullptr
        && destination.GetOwnedActionCountForAnalysis() == 0 && destroyed == 1,
        "copy clears destination current and owned container");
    Require(nested.GetClearableWordForAnalysis() == 0,
        "cleanup clears nested current action hook");
    Require(destination.GetOwnerForAnalysis() == reinterpret_cast<void*>(0x100)
        && destination.GetField24ForAnalysis() == reinterpret_cast<void*>(0x200)
        && destination.GetClearableWordForAnalysis() == 23,
        "base copy does not transfer or reset remaining payload");

    source.SetOwnerForAnalysis(reinterpret_cast<void*>(0x300));
    source.SetClearableWordForAnalysis(99);
    auto clone = source.Clone();
    auto* actionClone = dynamic_cast<wxAIAction*>(clone.get());
    Require(actionClone != nullptr && actionClone->GetOwnerForAnalysis() == nullptr
        && actionClone->GetClearableWordForAnalysis() == 0
        && actionClone->GetDurationMillisecondsForAnalysis() == 2000,
        "clone retains constructor defaults, not source payload");

    std::cout << "wxAIAction reconstruction tests passed\n";
    return EXIT_SUCCESS;
}
