#include "spPS2InputDevice.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord PS2InputDeviceRecord{spPS2InputDevice::ClassID, spInputDevice::ClassID,
            "spPS2InputDevice", &spInputDevice::StaticRTTI(), nullptr, nullptr};
        const bool PS2InputDeviceRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2InputDeviceRecord);
    }
    const spRTTIRecord& spPS2InputDevice::StaticRTTI() noexcept
    { (void)PS2InputDeviceRegistered; return PS2InputDeviceRecord; }
    const spRTTIRecord& spPS2InputDevice::vfunc_18() const noexcept { return PS2InputDeviceRecord; }
}
