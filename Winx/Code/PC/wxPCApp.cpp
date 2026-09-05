#include "wxPCApp.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spPCApp;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCApp()
        {
            return std::make_unique<wxPCApp>();
        }

        const spRTTIRecord PCAppRecord{
            wxPCApp::ClassID,
            spPCApp::ClassID,
            "wxPCApp",
            &spPCApp::StaticRTTI(),
            &CreatePCApp,
            nullptr,
        };
    }

    wxPCApp::~wxPCApp() = default;

    const spRTTIRecord& wxPCApp::StaticRTTI() noexcept
    {
        return PCAppRecord;
    }

    std::unique_ptr<spBaseObject> wxPCApp::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxPCApp>();
        // Native uses the actual C++ spNamedObject copy path even though the
        // engine RTTI chain deliberately flattens spApp to spBaseObject.
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        clone->SetName(GetName());
        return clone;
    }

    const spRTTIRecord& wxPCApp::vfunc_18() const noexcept
    {
        return PCAppRecord;
    }

    const char* wxPCApp::vfunc_GetEmptyString() const noexcept
    {
        return windowTitle_.c_str();
    }

    bool wxPCApp::vfunc_20_Initialize()
    {
        // Native 0x0040E2F0 creates and connects many game-wide managers
        // before delegating to spPCApp window creation.  No process-global or
        // GUI side effects are attempted by this isolated reconstruction.
        return false;
    }

    bool wxPCApp::vfunc_24_Update()
    {
        // Native 0x0040E610 advances the engine core, input and game state,
        // then delegates to spPCApp.  Missing managers are a contained stop.
        return false;
    }

    void wxPCApp::vfunc_28_Shutdown()
    {
        // Native 0x0040E180 sets byte +0x96 before its manager shutdown calls.
        shutdownRequested_ = true;
        spPCApp::vfunc_28_Shutdown();
    }

    void wxPCApp::SetBuildLabelsForAnalysis(
        const std::string_view version,
        const std::string_view buildLabel)
    {
        windowTitle_ = "Winx PC Version ";
        windowTitle_.append(version);
        windowTitle_.append(" - ");
        windowTitle_.append(buildLabel);
    }

    bool wxPCApp::IsShutdownRequestedForAnalysis() const noexcept
    {
        return shutdownRequested_;
    }
}
