#pragma once

// Native PC class identity; original header/translation-unit path is unknown.
#include "../Sparkplug/spTextureManager.h"

namespace sparkplug::reconstruction
{
    class spDXTextureManager final : public spTextureManager
    {
    public:
        static constexpr spClassID ClassID = 0x3B9304C7;

        spDXTextureManager() noexcept = default;
        ~spDXTextureManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        // PC primary slots +1C/+20 both target the literal RET at0048EAA0.
        // They perform no device call, mutation or resource release.
        void NativeHook1CForAnalysis() noexcept override;
        void NativeHook20ForAnalysis() noexcept override;
    };
}
