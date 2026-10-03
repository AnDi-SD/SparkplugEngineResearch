#include "wxIceWormMovingState.h"
#include "Analysis/Host/wxIceWormMovingStateHost.h"
#include <cmath>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceWormMovingState>(); }
        const spRTTIRecord record{wxIceWormMovingState::ClassID, wxCharacterState::ClassID,
            "wxIceWormMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxIceWormMovingStateHost& RequireMovingHost(wxCharacterStateHost& base)
        {
            auto* host = dynamic_cast<wxIceWormMovingStateHost*>(&base);
            if (!host) throw std::logic_error("wxIceWormMovingState requires a motion/turn/notification host");
            return *host;
        }
    }
    wxIceWormMovingState::wxIceWormMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceWormMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceWormMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceWormMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceWormMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxIceWormMovingState::NotifyFromState(const std::uint32_t code)
    {
        auto& host = RequireMovingHost(RequireHostForAnalysis());
        void* const receiver = host.OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host.SendMovingNotificationForAnalysis(receiver, *this, code, 0x6E);
    }
    bool wxIceWormMovingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        NotifyFromState(0x27D1);
        return wxCharacterState::vfunc_1C(request);
    }
    bool wxIceWormMovingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        NotifyFromState(0x27D2);
        return wxCharacterState::vfunc_20(request);
    }
    void wxIceWormMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC519350 / PS22ECE70. Capture both external objects before the
        // branch; the angular branch resolves the turn object again.
        auto& host = RequireMovingHost(RequireHostForAnalysis());
        const auto objects = host.OwnerEntityObjectsForAnalysis(GetOwnerForAnalysis());
        const bool ps2 = host.NumericProfileForAnalysis() == wxIceWormMovingNumericProfileForAnalysis::PS2Finite;
        bool useMotion = objects.turnObject == nullptr;
        if (!useMotion)
        {
            const float first = host.ReadTurnWordForAnalysis(objects.turnObject, 0x164);
            const float second = host.ReadTurnWordForAnalysis(objects.turnObject, 0x1A0);
            // PC compares before the float32 spill; PS2 SUB.S rounds first.
            // For the small float32 threshold comparison, binary64 retains
            // all significant bits that can affect the PC branch decision.
            const double difference = static_cast<double>(first) - static_cast<double>(second);
            const double compared = ps2 ? static_cast<double>(static_cast<float>(difference)) : difference;
            useMotion = std::fabs(compared) <= static_cast<double>(0.1745f);
        }
        if (useMotion)
        {
            const float motion = host.ReadMotionForAnalysis(objects.motionObject);
            request.packedKey = motion < 0.2f ? request.packedKey & 0xFFFFFF8Fu
                : (request.packedKey & 0xFFFFFF9Fu) | 0x10u;
        }
        else
        {
            void* const turn = host.ReadOwnerTurnObjectForAnalysis(GetOwnerForAnalysis());
            float first, second;
            if (ps2)
            {
                first = host.ReadTurnWordForAnalysis(turn, 0x164);
                second = host.ReadTurnWordForAnalysis(turn, 0x1A0);
            }
            else
            {
                second = host.ReadTurnWordForAnalysis(turn, 0x1A0);
                first = host.ReadTurnWordForAnalysis(turn, 0x164);
            }
            float delta = static_cast<float>(static_cast<double>(first) - static_cast<double>(second));
            host.NormalizeAngleForAnalysis(delta);
            request.packedKey = delta < 0.0f ? (request.packedKey & 0xFFFFFFBFu) | 0x30u
                : (request.packedKey & 0xFFFFFFCFu) | 0x40u;
        }
        request.packedKey &= 0xF01FFFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
