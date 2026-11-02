#include "wxMikaelWandringAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxPlanarAngleMath.h"
#include "Code/Sparkplug/spNode.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMikaelWandring()
        { throw std::logic_error("Mikael Entry requires captured state and explicit borrowed bindings"); }
        const spRTTIRecord record{wxMikaelWandringAIAction::ClassID, wxAIAction::ClassID,
            "wxMikaelWandringAIAction", &wxAIAction::StaticRTTI(), &CreateMikaelWandring, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        std::uint32_t FloatBits(const float value) noexcept
        { std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
    }
    wxMikaelWandringAIAction::wxMikaelWandringAIAction(wxMikaelWandringAIActionHost& host,
        const wxMikaelWandringAIActionCapturedStateForAnalysis& state, const ProfileForAnalysis profile) noexcept
        : host_(host), profile_(profile), state_(state) {}
    const spRTTIRecord& wxMikaelWandringAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMikaelWandringAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMikaelWandringAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Mikael Clone remains unqualified"); }
    bool wxMikaelWandringAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    bool wxMikaelWandringAIAction::IsProfileAdmittedForAnalysis() const noexcept
    {
        return profile_ == ProfileForAnalysis::PcCapturedFiniteEntryAndOwn48
            && GetCurrentActionForAnalysis() == nullptr && GetOwnedActionCountForAnalysis() == 0;
    }
    wxMikaelWandringAIActionResultForAnalysis wxMikaelWandringAIAction::OwnSlot48ForAnalysis() noexcept
    {
        using Result = wxMikaelWandringAIActionResultForAnalysis;
        using CommandResult = wxAICommandRecordForAnalysis::ResultForAnalysis;
        if (!IsProfileAdmittedForAnalysis()) return lastResult_ = Result::UnqualifiedProfile;
        // Actual body copies target WORLDposition first, then source position
        // and raw WORLDorientation row2. Scale and LOCAL caches are not read.
        if (!state_.borrowedTargetNode3B4) return lastResult_ = Result::MissingTargetNode;
        const auto targetPosition = state_.borrowedTargetNode3B4->GetWorldPositionForAnalysis();
        if (!state_.borrowedSourceNode3B0) return lastResult_ = Result::MissingSourceNode;
        const auto sourcePosition = state_.borrowedSourceNode3B0->GetWorldPositionForAnalysis();
        const auto& orientation = state_.borrowedSourceNode3B0->GetWorldOrientationForAnalysis();
        const analysis::pc::PlanarVectorForAnalysis forward{orientation[6], orientation[7], orientation[8]};
        if (!analysis::pc::IsBoundedPlanarVectorForAnalysis(forward))
            return lastResult_ = Result::UnqualifiedGeometry;
        // Unlike shared strict Facing, Mikael does not normalize forward.
        const float currentYaw = float(analysis::pc::PcPlanarVectorYawForAnalysis(forward));
        float desiredYaw;
        if (!analysis::pc::PcPlanarPositionYawForAnalysis(sourcePosition, targetPosition, desiredYaw))
            return lastResult_ = Result::UnqualifiedGeometry;
        const double distance = analysis::pc::PcPlanarAngleDistanceForAnalysis(currentYaw, desiredYaw);
        auto* command = state_.borrowedBaseCommand3A4;
        if (!command) return lastResult_ = Result::MissingCommand;
        // Saved desiredYaw is published before the fresh command reload.
        // Wrapping argument copies inside5971A0 cannot change this float.
        if (command->StoreWord08ForAnalysis(FloatBits(desiredYaw)) != CommandResult::Completed)
            return lastResult_ = Result::CommandNotAdmitted;
        command = state_.borrowedBaseCommand3A4;
        if (!command) return lastResult_ = Result::MissingCommand;
        const auto motion = distance <= double(0.08f) ? 0x3F800000u : 0x3D4CCCCDu;
        if (command->StoreWord04ForAnalysis(motion) != CommandResult::Completed)
            return lastResult_ = Result::CommandNotAdmitted;
        return lastResult_ = Result::Completed;
    }
    wxMikaelWandringAIActionResultForAnalysis wxMikaelWandringAIAction::EnterForAnalysis() noexcept
    {
        using Result = wxMikaelWandringAIActionResultForAnalysis;
        if (OwnSlot48ForAnalysis() != Result::Completed) return lastResult_;
        auto* command = state_.borrowedBaseCommand3A4;
        if (!command) return lastResult_ = Result::MissingCommand;
        if (command->StoreByte1BForAnalysis(1) != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
            return lastResult_ = Result::CommandNotAdmitted;
        // Native Entry captures owner only after the completed command stores.
        auto* const owner = GetOwnerForAnalysis();
        if (!owner) return lastResult_ = Result::MissingOwner;
        auto* const current = host_.BorrowOwnerWord1C0ForAnalysis(owner);
        if (!current) return lastResult_ = Result::MissingOwnerStorage;
        const auto capturedCurrent = *current;
        auto* const previous = host_.BorrowOwnerWord1BCForAnalysis(owner);
        if (!previous) return lastResult_ = Result::MissingOwnerStorage;
        *previous = capturedCurrent;
        *current = 1;
        return lastResult_ = Result::Completed;
    }
    void wxMikaelWandringAIAction::vfunc_30() noexcept { (void)EnterForAnalysis(); }
    wxMikaelWandringAIActionResultForAnalysis wxMikaelWandringAIAction::OwnSlot4CForAnalysis(
        const Own4CProfileForAnalysis profile) noexcept
    {
        using Result = wxMikaelWandringAIActionResultForAnalysis;
        using CommandResult = wxAICommandRecordForAnalysis::ResultForAnalysis;
        if (profile != Own4CProfileForAnalysis::PcCapturedFiniteStoredPoint
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = Result::UnqualifiedProfile;
        if (!state_.borrowedSourceNode3B0) return lastResult_ = Result::MissingSourceNode;
        const auto sourcePosition = state_.borrowedSourceNode3B0->GetWorldPositionForAnalysis();
        const auto& orientation = state_.borrowedSourceNode3B0->GetWorldOrientationForAnalysis();
        const analysis::pc::PlanarVectorForAnalysis forward{orientation[6], orientation[7], orientation[8]};
        if (!analysis::pc::IsBoundedPlanarVectorForAnalysis(forward))
            return lastResult_ = Result::UnqualifiedGeometry;
        const float currentYaw = float(analysis::pc::PcPlanarVectorYawForAnalysis(forward));
        // Actual5970B0 receives &own3B8 directly as its target argument. The
        // raw input stays unchanged; its producer/Init remain unqualified.
        analysis::pc::PlanarVectorForAnalysis storedPoint;
        std::memcpy(storedPoint.data(), state_.ownBytes3B8.data(), sizeof(storedPoint));
        float desiredYaw;
        if (!analysis::pc::PcPlanarPositionYawForAnalysis(sourcePosition, storedPoint, desiredYaw))
            return lastResult_ = Result::UnqualifiedGeometry;
        const double distance = analysis::pc::PcPlanarAngleDistanceForAnalysis(currentYaw, desiredYaw);
        auto* command = state_.borrowedBaseCommand3A4;
        if (!command) return lastResult_ = Result::MissingCommand;
        if (command->StoreWord08ForAnalysis(FloatBits(desiredYaw)) != CommandResult::Completed)
            return lastResult_ = Result::CommandNotAdmitted;
        command = state_.borrowedBaseCommand3A4;
        if (!command) return lastResult_ = Result::MissingCommand;
        if (command->StoreWord04ForAnalysis(distance <= double(0.08f) ? 0x3F800000u : 0x3D4CCCCDu)
            != CommandResult::Completed) return lastResult_ = Result::CommandNotAdmitted;
        return lastResult_ = Result::Completed;
    }
    void wxMikaelWandringAIAction::vfunc_0C(const void*) noexcept { lastResult_ = wxMikaelWandringAIActionResultForAnalysis::UnqualifiedNotify; }
    bool wxMikaelWandringAIAction::RefuseHookForAnalysis() noexcept { lastResult_ = wxMikaelWandringAIActionResultForAnalysis::UnqualifiedHook; return false; }
    bool wxMikaelWandringAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxMikaelWandringAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxMikaelWandringAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxMikaelWandringAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxMikaelWandringAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxMikaelWandringAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxMikaelWandringAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
