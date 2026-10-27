#pragma once
#include "Code/wxEntity.h"

namespace winx::reconstruction
{
    // Partial post-construction data profile. This constructor does not
    // reproduce the native7192-byte controller's constructor or resources.
    class wxCharacterMoveCtrl final:public wxEntity
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x680838E4;
        static constexpr std::size_t PcOwnStartForAnalysis=0x124,PcExtentForAnalysis=0x1C18;
        using OwnBytesForAnalysis=std::array<std::uint8_t,PcExtentForAnalysis-PcOwnStartForAnalysis>;
        struct CapturedPostStateForAnalysis final
        {
            std::uint32_t entityField20;
            wxEntity::OwnBytesForAnalysis entityFields;
            OwnBytesForAnalysis controllerFields;
        };
        explicit wxCharacterMoveCtrl(wxEntityHost&,const CapturedPostStateForAnalysis&);
        // Portable byte storage/base cleanup only. Native derived teardown is open.
        ~wxCharacterMoveCtrl() override=default;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        // Analytical refusals: inherited base-only success would drop own fields.
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,sparkplug::reconstruction::spCloneManager&) const override;
        void ResetFieldsForAnalysis() noexcept;
        const OwnBytesForAnalysis& GetControllerBytesForAnalysis() const noexcept { return own_; }
    private:
        OwnBytesForAnalysis own_;
    };
}
