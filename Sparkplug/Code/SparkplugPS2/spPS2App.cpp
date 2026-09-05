#include "spPS2App.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord PS2AppRecord{
            spPS2App::ClassID,
            spApp::ClassID,
            "spPS2App",
            &spApp::StaticRTTI(),
            nullptr,
            nullptr,
        };
    }

    spPS2App::~spPS2App() = default;

    const spRTTIRecord& spPS2App::StaticRTTI() noexcept
    {
        return PS2AppRecord;
    }

    std::unique_ptr<spBaseObject> spPS2App::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spPS2App::vfunc_18() const noexcept
    {
        return PS2AppRecord;
    }

    bool spPS2App::vfunc_20_Initialize()
    {
        // Secondary-table target 0x001E76B0 returns true immediately.
        return true;
    }

    void spPS2App::vfunc_28_Shutdown()
    {
        // Secondary-table target 0x001E7650 is an immediate no-op.
    }

    bool spPS2App::vfunc_2C_Run()
    {
        // 0x001E7660 repeatedly calls the concrete update hook until it
        // returns false, then marks termination.  There is no native success
        // return from this loop; the zero result is preserved.
        while (vfunc_24_Update())
        {
        }
        SetStateFlagForAnalysis(true);
        return false;
    }
}
