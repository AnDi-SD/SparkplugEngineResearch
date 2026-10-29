#pragma once

#include "../Sparkplug/spAudioManager.h"

namespace sparkplug::reconstruction
{
    class spDXAudioManager final : public spAudioManager
    {
    public:
        static constexpr spClassID ClassID=0x116F7E16;
        // Explicit analytical bank-registry capture. This is not original
        // factory4C4200, the device backend or the global manager lifetime.
        explicit spDXAudioManager(spAudioManagerCapturedPcRegistryForAnalysis&&);
        ~spDXAudioManager() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // Native has factory4C4200. Portable RTTI deliberately has no factory:
        // creation without an explicit captured profile remains unsupported.
    };
}
