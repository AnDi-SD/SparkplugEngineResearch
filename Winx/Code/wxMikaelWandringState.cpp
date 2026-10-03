#include "wxMikaelWandringState.h"
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMikaelWandringState>(); }
        const spRTTIRecord record{wxMikaelWandringState::ClassID, wxCharacterState::ClassID,
            "wxMikaelWandringState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMikaelWandringState::wxMikaelWandringState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMikaelWandringState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMikaelWandringState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMikaelWandringState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMikaelWandringState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMikaelWandringState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { return wxNPCStateOperationsForAnalysis::Enter(*this, request); }
    void wxMikaelWandringState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5A8080 / paired PS2: same-handle bypasses query and word write.
        request.packedKey = (request.packedKey & 0xF0000050u) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        if (fields_.RequireField3C())
            fields_.field3C = static_cast<std::uint8_t>(!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true));
        if (!fields_.RequireField3C())
        {
            // Retain original mode40 branch, even though stable request50
            // selects mode-one in the ordinary update path.
            const bool modeZero = (request.packedKey & 0x70u) == 0x40u;
            if (modeZero) fields_.field3C = std::uint8_t{1};
            ReleasePendingFromState();
            QueuePendingFromState(handle, !modeZero, true);
            SetPendingHandleFromState(handle);
            return;
        }
        auto* host = dynamic_cast<wxNPCStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("MikaelWandring state requires a control-word host");
        host->WriteOwnerActionControlForAnalysis(GetOwnerForAnalysis(), 0x3DCCCCCDu);
    }
    bool wxMikaelWandringState::vfunc_34(std::uint32_t)
    { return fields_.RequireField3C() == 0; }
}
