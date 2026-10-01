#include "wxDispelState.h"
#include "Analysis/Host/wxDispelStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDispelState()
        { return std::make_unique<wxDispelState>(); }
        const spRTTIRecord record{wxDispelState::ClassID, wxCharacterState::ClassID,
            "wxDispelState", &wxCharacterState::StaticRTTI(), &CreateDispelState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxDispelStateHost& RequireDispelHost(wxCharacterStateHost& host)
        {
            auto* adapter = dynamic_cast<wxDispelStateHost*>(&host);
            if (!adapter) throw std::logic_error("wxDispelState requires a dispel-state host");
            return *adapter;
        }
    }
    const spRTTIRecord& wxDispelState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxDispelState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDispelState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDispelState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDispelState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey &= 0xFF807F80u;
        exitMessageFlag_ = (request.packedKey & 0x0F800000u) == 0;
        auto& host = RequireDispelHost(RequireHostForAnalysis());
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        // PC5A6683 / PS22CB408: old handle is not released before queuing.
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        host.ClearOwnerControlWord68ForAnalysis(GetOwnerForAnalysis());
        ClearOwnerActionControlFromState();
        return true;
    }
    bool wxDispelState::vfunc_20(wxAnimationRequestForAnalysis&)
    {
        if (exitMessageFlag_)
        {
            auto& host = RequireDispelHost(RequireHostForAnalysis());
            if (host.HasNotificationManagerForAnalysis())
            {
                wxDispelStateMessageForAnalysis message;
                message.source = this;
                host.DispatchExitMessageForAnalysis(message);
            }
        }
        ReleasePendingFromState();
        return true;
    }
    void wxDispelState::vfunc_30(wxAnimationRequestForAnalysis&)
    { ClearOwnerActionControlFromState(); }
    bool wxDispelState::vfunc_34(std::uint32_t)
    {
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
