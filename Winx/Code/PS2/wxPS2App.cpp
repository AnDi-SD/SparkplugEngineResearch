#include "wxPS2App.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spPS2App;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2App()
        {
            return std::make_unique<wxPS2App>();
        }

        const spRTTIRecord PS2AppRecord{
            wxPS2App::ClassID,
            spPS2App::ClassID,
            "wxPS2App",
            &spPS2App::StaticRTTI(),
            &CreatePS2App,
            nullptr,
        };
    }

    wxPS2App::~wxPS2App() = default;

    const spRTTIRecord& wxPS2App::StaticRTTI() noexcept
    {
        return PS2AppRecord;
    }

    std::unique_ptr<spBaseObject> wxPS2App::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxPS2App>();
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        clone->SetName(GetName());
        return clone;
    }

    const spRTTIRecord& wxPS2App::vfunc_18() const noexcept
    {
        return PS2AppRecord;
    }

    const char* wxPS2App::vfunc_GetEmptyString() const noexcept
    {
        // Exact literal returned by native 0x003E50A0.
        return "Winx Club (PS2) - Build 0.00.08";
    }

    bool wxPS2App::vfunc_20_Initialize()
    {
        // Native 0x003E53D0 wires global managers and platform services.  The
        // portable slice contains those absent dependencies explicitly.
        return false;
    }

    bool wxPS2App::vfunc_24_Update()
    {
        // Native 0x003E50B0 updates timing, engine core and display state.
        return false;
    }

    void wxPS2App::vfunc_28_Shutdown()
    {
        // The native override releases four global services, delegates to
        // spPS2App, and clears one GP-relative byte.  There are no owned host
        // resources in this safe facade.
        spPS2App::vfunc_28_Shutdown();
    }
}
