#include "wxSpiderMovingState.h"
#include "Analysis/Host/wxCharacterMotionStateHost.h"
#include "Analysis/Host/wxCharacterSpeedStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxSpiderMovingState>(); }
        const spRTTIRecord record{wxSpiderMovingState::ClassID, wxCharacterState::ClassID,
            "wxSpiderMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxSpiderMovingState::wxSpiderMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxSpiderMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxSpiderMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxSpiderMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxSpiderMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxSpiderMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto& binding = RequireHostForAnalysis();
        auto* motionHost = dynamic_cast<wxCharacterMotionStateHost*>(&binding);
        auto* speedHost = dynamic_cast<wxCharacterSpeedStateHost*>(&binding);
        if (!motionHost || !speedHost) throw std::logic_error("wxSpiderMovingState requires motion and speed hosts");
        const float motion = motionHost->OwnerMotionForAnalysis(GetOwnerForAnalysis());
        // PC520940 / PS22F2380: equality and PC unordered select movement.
        if (motion < 0.1f)
        {
            speedHost->SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            request.packedKey &= 0xFFFFFF8Fu;
            ClearOwnerActionControlFromState();
        }
        else
        {
            speedHost->SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), motion);
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        }
        request.packedKey &= 0xF007FFFFu;
        void* const handle = binding.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
