#include "Analysis/PC/WinxAppAbi.h"
#include "Analysis/PS2/WinxAppAbi.h"
#include "Code/PS2/wxPS2App.h"
#include "Code/wxEngineCore.h"
#if defined(_WIN32)
#include "Code/PC/wxPCApp.h"
#endif

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
    void Require(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    struct OrderedProbe final
    {
        int* sequence;
        int digit;
    };

    void AppendProbe(void* context)
    {
        auto& probe = *static_cast<OrderedProbe*>(context);
        *probe.sequence = *probe.sequence * 10 + probe.digit;
    }

    bool AppendBaseProbe(void* context)
    {
        auto& probe = *static_cast<OrderedProbe*>(context);
        *probe.sequence = *probe.sequence * 10 + probe.digit;
        return true;
    }
}

int main()
{
    using namespace winx::reconstruction;
    using sparkplug::reconstruction::spApp;

    Require(sizeof(winx::evidence::ps2::wxEngineCoreLayout) == 0x150,
        "PS2 wxEngineCore adds no storage to spEngineCore");
    Require(sizeof(winx::evidence::pc::wxEngineCoreObservedPrefixLayout) == 0x158,
        "PC wxEngineCore preserves the complete observed base prefix");

    {
        wxEngineCore core;
        Require(wxEngineCore::StaticRTTI().classID == wxEngineCore::ClassID
                && wxEngineCore::StaticRTTI().baseClassID
                    == sparkplug::reconstruction::spEngineCore::ClassID,
            "wxEngineCore native identity and engine base are preserved");
        Require(sparkplug::reconstruction::spEngineCore::GetInstance() == &core,
            "game core construction publishes the inherited singleton");

        int sequence = 0;
        OrderedProbe first{&sequence, 1};
        OrderedProbe second{&sequence, 2};
        OrderedProbe base{&sequence, 3};
        const wxEngineCore::AnalysisActionBinding actions[]{
            {&AppendProbe, &first},
            {&AppendProbe, &second},
        };
        core.SetFrameCallbacksForAnalysis(nullptr, nullptr, &AppendBaseProbe, &base);
        Require(core.RunFrameBoundaryForAnalysis(actions, 2),
            "wx frame boundary returns the delegated engine result");
        Require(sequence == 123,
            "game actions run before the inherited engine boundary");

        auto clone = core.Clone();
        Require(dynamic_cast<wxEngineCore*>(clone.get()) != nullptr,
            "wxEngineCore clone creates the concrete game type");
    }
    Require(sparkplug::reconstruction::spEngineCore::GetInstance() == nullptr,
        "game core destruction clears the inherited singleton");

    Require(sizeof(winx::evidence::ps2::wxPS2AppLayout) == 0x48,
        "PS2 factory allocation layout is exact");
    Require(sizeof(winx::evidence::pc::wxPCAppAllocationLayout) == 0x2CC,
        "PC factory allocation layout is exact");

    {
        wxPS2App app;
        Require(wxPS2App::StaticRTTI().classID == wxPS2App::ClassID
                && wxPS2App::StaticRTTI().baseClassID
                    == sparkplug::reconstruction::spPS2App::ClassID,
            "wxPS2App native identity and base are preserved");
        Require(std::strcmp(app.vfunc_GetEmptyString(),
                    "Winx Club (PS2) - Build 0.00.08") == 0,
            "wxPS2App returns the exact native build-title literal");
        Require(!app.vfunc_20_Initialize() && !app.vfunc_24_Update(),
            "missing PS2 globals are contained at the portable boundary");
        auto clone = app.Clone();
        Require(dynamic_cast<wxPS2App*>(clone.get()) != nullptr,
            "wxPS2App clone slot creates the concrete game type");
    }
    Require(spApp::GetInstance() == nullptr,
        "PS2 game app destruction clears the inherited singleton");

#if defined(_WIN32)
    {
        wxPCApp app;
        Require(wxPCApp::StaticRTTI().classID == wxPCApp::ClassID
                && wxPCApp::StaticRTTI().baseClassID
                    == sparkplug::reconstruction::spPCApp::ClassID,
            "wxPCApp native identity and base are preserved");
        app.SetBuildLabelsForAnalysis("1.00", "test");
        Require(std::strcmp(app.vfunc_GetEmptyString(),
                    "Winx PC Version 1.00 - test") == 0,
            "PC title seam follows the native formatting template");
        Require(!app.vfunc_20_Initialize() && !app.vfunc_24_Update(),
            "missing PC game managers are contained at the portable boundary");
        app.vfunc_28_Shutdown();
        Require(app.IsShutdownRequestedForAnalysis(),
            "wxPCApp shutdown records the native +0x96 transition");
        auto clone = app.Clone();
        Require(dynamic_cast<wxPCApp*>(clone.get()) != nullptr,
            "wxPCApp clone slot creates the concrete game type");
    }
    Require(spApp::GetInstance() == nullptr,
        "PC game app destruction clears the inherited singleton");
#endif

    Require(wxPS2App::ClassID == winx::evidence::ps2::wxPS2AppClassID,
        "portable and PS2 wxPS2App IDs agree");
#if defined(_WIN32)
    Require(wxPCApp::ClassID == winx::evidence::pc::wxPCAppClassID,
        "portable and PC wxPCApp IDs agree");
#endif
    Require(wxEngineCore::ClassID == winx::evidence::pc::wxEngineCoreClassID
            && wxEngineCore::ClassID == winx::evidence::ps2::wxEngineCoreClassID,
        "portable and native wxEngineCore IDs agree");
    Require(winx::evidence::pc::wxEngineCoreOverrideOrdinal == 10
            && winx::evidence::ps2::wxEngineCoreOverrideOrdinal == 10,
        "both platforms override the same sole engine-core slot");

    std::cout << "Winx app reconstruction tests passed\n";
    return EXIT_SUCCESS;
}
