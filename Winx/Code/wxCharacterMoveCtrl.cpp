#include "wxCharacterMoveCtrl.h"
#include "Analysis/PC/wxCharacterMoveCtrlAbi.h"
#include <cstring>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        // Null portable factory means unsupported construction, not that the
        // actual registered native class is abstract or lacks a factory.
        const spRTTIRecord record{wxCharacterMoveCtrl::ClassID,wxEntity::ClassID,"wxCharacterMoveCtrl",
            &wxEntity::StaticRTTI(),nullptr,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxCharacterMoveCtrl::wxCharacterMoveCtrl(wxEntityHost& host,const CapturedPostStateForAnalysis& captured)
        :wxEntity(host,captured.entityField20),own_(captured.controllerFields)
    {
        wxEntity::GetOwnBytesForAnalysis()=captured.entityFields;
    }
    const spRTTIRecord& wxCharacterMoveCtrl::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxCharacterMoveCtrl::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxCharacterMoveCtrl::vfunc_10(spCloneManager&) const { return {}; }
    bool wxCharacterMoveCtrl::vfunc_14(spBaseObject&,spCloneManager&) const { return false; }
    void wxCharacterMoveCtrl::ResetFieldsForAnalysis() noexcept
    {
        // Original4D96A0 executes the first1BC store through its protected
        // bridge, then the fourteen visible stores. All15 were observed on
        // actual factory-backed receivers; raw remaining bytes are input data.
        const std::uint32_t zero=0;
        for (const auto offset:evidence::pc::wxCharacterMoveCtrlResetFields)
            std::memcpy(own_.data()+offset-PcOwnStartForAnalysis,&zero,4);
    }
}
