#include "wxMinotaurAttackState.h"
#include "Analysis/Host/wxMinotaurAttackStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMinotaurAttackState>(); }
        const spRTTIRecord record{wxMinotaurAttackState::ClassID, wxCharacterState::ClassID,
            "wxMinotaurAttackState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxMinotaurAttackStateHost& RequireMinotaurHost(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxMinotaurAttackStateHost*>(&host);
            if (!result) throw std::logic_error("wxMinotaurAttackState requires a speed/notification host");
            return *result;
        }
        void Notify(wxMinotaurAttackStateHost& host, wxMinotaurAttackState& state, void* owner, bool flag)
        {
            void* const receiver = host.OwnerField24ForAnalysis(owner);
            if (receiver) host.SendFlagNotificationForAnalysis(receiver, state, flag);
        }
    }
    wxMinotaurAttackState::wxMinotaurAttackState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMinotaurAttackState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMinotaurAttackState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMinotaurAttackState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMinotaurAttackState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMinotaurAttackState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5213F0 / PS2304720. Own byte3C clears before release callbacks.
        if (GetTransitionFlag1C())
        {
            field3C_ = 0;
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xF0200450u) | 0x200450u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1C();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            Notify(RequireMinotaurHost(RequireHostForAnalysis()), *this, GetOwnerForAnalysis(), true);
            return wxCharacterState::vfunc_1C(request);
        }
        ClearOwnerActionControlFromState();
        return false;
    }
    bool wxMinotaurAttackState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC521580 / PS23042E0. External byte60 takes precedence over once1E.
        auto& host = RequireMinotaurHost(RequireHostForAnalysis());
        if (host.OwnerExitFlag60ForAnalysis(GetOwnerForAnalysis()))
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            Notify(host, *this, GetOwnerForAnalysis(), false);
            return wxCharacterState::vfunc_20(request);
        }
        if (GetTransitionFlag1E())
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xFFC00450u) | 0x400450u;
            const std::uint32_t variant = host.NumericProfileForAnalysis() == wxMinotaurAttackNumericProfileForAnalysis::PS2Finite
                ? field3C_ & 0x1Fu : std::uint32_t(field3C_ != 0);
            request.packedKey = (request.packedKey & 0xF07FFFFFu) | (variant << 23);
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            Notify(host, *this, GetOwnerForAnalysis(), false);
            return wxCharacterState::vfunc_20(request);
        }
        ClearOwnerActionControlFromState();
        return false;
    }
    void wxMinotaurAttackState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5214C0 / PS2304580. Speed object's borrowed identity is captured
        // before animation callbacks; numerator/denominator are read afterwards.
        auto& host = RequireMinotaurHost(RequireHostForAnalysis());
        void* const speedObject = host.OwnerSpeedObjectForAnalysis(GetOwnerForAnalysis());
        request.packedKey = (request.packedKey & 0xF0000450u) | 0x450u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState();
            QueuePendingFromState(handle, true, true);
            SetPendingHandleFromState(handle);
        }
        const float numerator = host.ReadSpeedNumeratorForAnalysis(speedObject);
        const float denominator = host.ReadSpeedDenominatorForAnalysis(speedObject);
        constexpr float minimum = 0.8f;
        float speed;
        if (host.NumericProfileForAnalysis() == wxMinotaurAttackNumericProfileForAnalysis::PS2Finite)
        {
            const float ratio = numerator / denominator;
            speed = minimum <= ratio ? ratio : minimum;
        }
        else
        {
            const double ratio = double(numerator) / double(denominator);
            speed = double(minimum) > ratio ? minimum : static_cast<float>(ratio);
        }
        host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), speed);
    }
}
