#include "wxStrafingState.h"
#include "Analysis/Host/wxStrafingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxStrafingState>(); }
        const spRTTIRecord record{wxStrafingState::ClassID, wxCharacterState::ClassID,
            "wxStrafingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxStrafingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxStrafingStateHost*>(&host);
            if (!result) throw std::logic_error("Strafing requires an observer and geometry host");
            return *result;
        }
        void Direction(wxAnimationRequestForAnalysis& request, std::uint32_t value)
        { request.packedKey = (request.packedKey & 0xFFFFFF8Fu) | value; }
    }
    wxStrafingState::wxStrafingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxStrafingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxStrafingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxStrafingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxStrafingState>(); manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxStrafingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (!field3C_)
        {
            field3C_ = host.ReadCachedStrafingObserverForAnalysis(GetOwnerForAnalysis());
            if (field3C_)
            {
                void* const observer = field3C_;
                // Capture new control before detaching the former binding.
                void* const previous = host.ReadObserverBindingForAnalysis(observer);
                void* const control = host.ReadStrafingEntityControlForAnalysis(GetOwnerForAnalysis());
                if (previous) host.RemoveStrafingObserverForAnalysis(previous, observer);
                host.WriteObserverBindingForAnalysis(observer, control);
                if (control) host.AddStrafingObserverForAnalysis(control, observer);
                host.WriteStrafingObserverWordForAnalysis(field3C_, 0x3C, 0x7F800000u);
                host.WriteStrafingObserverWordForAnalysis(field3C_, 0x44, 2);
                host.WriteStrafingObserverByte64ForAnalysis(field3C_, 0);
            }
        }
        if (GetTransitionFlag1C())
        {
            ClearTransitionFlag1C();
            if (!host.HasStrafingEntityField148ForAnalysis(GetOwnerForAnalysis()))
                host.SendStrafingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27E7, true, 0);
            void* const motion = host.ReadStrafingMotionObjectForAnalysis(GetOwnerForAnalysis());
            if (!(host.ReadStrafingMotionWordForAnalysis(motion, 4) < 0.2f))
                return wxCharacterState::vfunc_1C(request);
            request.packedKey = (request.packedKey & 0xF027FF8Fu) | 0x200000u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 2.0f);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle);
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            return wxCharacterState::vfunc_1C(request);
        }
        host.WriteStrafingDirectMotionForAnalysis(GetOwnerForAnalysis(), 0.01f); return false;
    }
    bool wxStrafingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        // Unregister precedes the transition-flag check on EVERY exit call.
        if (field3C_)
        {
            void* const observer = field3C_;
            void* const binding = host.ReadObserverBindingForAnalysis(observer);
            if (binding) host.RemoveStrafingObserverForAnalysis(binding, observer);
            host.WriteObserverBindingForAnalysis(observer, nullptr); field3C_ = nullptr;
        }
        if (GetTransitionFlag1E())
        {
            ClearTransitionFlag1E();
            if (!host.HasStrafingEntityField148ForAnalysis(GetOwnerForAnalysis()))
                host.SendStrafingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27E7, false, 0);
            void* const motion = host.ReadStrafingMotionObjectForAnalysis(GetOwnerForAnalysis());
            if (!(host.ReadStrafingMotionWordForAnalysis(motion, 4) < 0.2f))
            {
                host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
                return wxCharacterState::vfunc_20(request);
            }
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xF047800Fu) | 0x400000u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 2.0f);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle);
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            if (!host.HasStrafingEntityField148ForAnalysis(GetOwnerForAnalysis()))
                host.SendStrafingMessageForAnalysis(GetOwnerForAnalysis(), *this, 0x27E7, false, 0);
            host.SetConsumerSpeedForAnalysis(GetCompletionConsumerForAnalysis(), 1.0f);
            return wxCharacterState::vfunc_20(request);
        }
        ClearOwnerActionControlFromState(); return false;
    }
    void wxStrafingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        void* const motion = host.ReadStrafingMotionObjectForAnalysis(GetOwnerForAnalysis());
        const float first = host.ReadStrafingMotionWordForAnalysis(motion, 0x14);
        const float second = host.ReadStrafingMotionWordForAnalysis(motion, 8);
        const bool pc = host.NumericProfileForAnalysis() == wxStrafingNumericProfileForAnalysis::PC;
        float angle = pc ? static_cast<float>(double(first) + double(second)) : first + second;
        host.NormalizeStrafingAngleForAnalysis(angle);
        void* const geometryOwner = GetOwnerForAnalysis();
        auto right = host.ReadStrafingBasisForAnalysis(geometryOwner, 0); right[1] = 0;
        host.NormalizeStrafingBasisForAnalysis(right);
        auto forward = host.ReadStrafingBasisForAnalysis(pc ? geometryOwner : GetOwnerForAnalysis(), 2); forward[1] = 0;
        host.NormalizeStrafingBasisForAnalysis(forward);
        const float rightAngle = host.InvokeStrafingVectorAngleForAnalysis(right);
        const float rightPair = host.InvokeStrafingAnglePairForAnalysis(rightAngle, angle);
        const float forwardAngle = host.InvokeStrafingVectorAngleForAnalysis(forward);
        const float forwardPair = host.InvokeStrafingAnglePairForAnalysis(forwardAngle, angle);
        constexpr float low = 0.78539818525314331055f;
        const double high = pc ? double(low) * 3.0 : double(2.35619449615478515625f);
        if (host.ReadStrafingMotionWordForAnalysis(motion, 4) < 0.2f) Direction(request, 0);
        else if (forwardPair <= low) Direction(request, 0x10);
        else if (double(forwardPair) >= high) Direction(request, 0x20);
        else if (double(rightPair) >= high) Direction(request, 0x30);
        else if (rightPair <= low) Direction(request, 0x40);
        else { Direction(request, 0); ClearOwnerActionControlFromState(); }
        request.packedKey &= 0xF01FFFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState(); QueuePendingFromState(handle, true, true); SetPendingHandleFromState(handle);
    }
}
