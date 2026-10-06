#include "spDXInputDevice.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord DXInputDeviceRecord{spDXInputDevice::ClassID, spInputDevice::ClassID,
            "spDXInputDevice", &spInputDevice::StaticRTTI(), nullptr, nullptr};
        const bool DXInputDeviceRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(DXInputDeviceRecord);
    }
    spDXInputDevice::spDXInputDevice(ForeignReferencesForAnalysis& references) : references_(references)
    {
        state_.inputInterface4C = references_.GetInputInterface();
        if (!state_.inputInterface4C)
            throw std::invalid_argument("Original input manager interface is not supplied");
        references_.AddReference(state_.inputInterface4C);
    }
    spDXInputDevice::~spDXInputDevice()
    {
        // Literal order and re-reads from original 004D63E0. Release callbacks
        // can observe and modify state; no cached device hides those changes.
        if (state_.inputInterface4C) references_.ReleaseReference(state_.inputInterface4C);
        if (state_.device50)
        {
            if (state_.acquired45) references_.Unacquire(state_.device50);
            references_.ReleaseReference(state_.device50);
        }
    }
    const spRTTIRecord& spDXInputDevice::StaticRTTI() noexcept
    { (void)DXInputDeviceRegistered; return DXInputDeviceRecord; }
    const spRTTIRecord& spDXInputDevice::vfunc_18() const noexcept { return DXInputDeviceRecord; }
}
