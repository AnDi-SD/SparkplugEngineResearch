#pragma once
#include "Analysis/Host/wxAnimationLoaderHost.h"

namespace winx::reconstruction
{
    // Native wxEntity descendant without additional fields. Portable ancestry
    // stops at the existing spNamedObject; missing base behavior requires host.
    class wxAnimationLoader : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x7ABF4B41;
        explicit wxAnimationLoader(wxAnimationLoaderHost&);
        ~wxAnimationLoader() override;
        wxAnimationLoader(const wxAnimationLoader&) = delete;
        wxAnimationLoader& operator=(const wxAnimationLoader&) = delete;
        static void SetFactoryHostForAnalysis(wxAnimationLoaderHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        // Only the first uint32 (notification code) is read.
        void vfunc_0C(const void* notification) noexcept override;
        void ApplyCurrentLevelForAnalysis(std::uint8_t load) noexcept;
        void ApplySetForAnalysis(std::uint32_t set, std::uint8_t load) noexcept;
    private:
        wxAnimationLoaderHost& host_;
    };
}
