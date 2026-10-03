#include "wxVulnerableState.h"
#include "Analysis/Host/wxVulnerableStateHost.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxVulnerableState>(); }
        const spRTTIRecord record{wxVulnerableState::ClassID, wxCharacterState::ClassID,
            "wxVulnerableState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxVulnerableStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxVulnerableStateHost*>(&host);
            if (!result) throw std::logic_error("VulnerableState requires an event/exit field adapter");
            return *result;
        }
    }
    wxVulnerableState::wxVulnerableState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxVulnerableState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxVulnerableState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxVulnerableState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxVulnerableState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxVulnerableState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        if (GetStateSelectorForAnalysis() == 33 && !field3D_)
            return wxCharacterState::vfunc_1C(request);
        if (GetTransitionFlag1C())
        {
            request.packedKey = (request.packedKey & 0xF120800Fu) | 0x01208000u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            ReleasePendingFromState();
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            field3C_ = 0;
            ClearTransitionFlag1C();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_1C(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    void wxVulnerableState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        field3C_ = 1;
        request.packedKey = (request.packedKey & 0xFF80800Fu) | 0x8000u;
        if (GetStateSelectorForAnalysis() == 33)
            request.packedKey = (request.packedKey & 0xF1FFFFFFu) | 0x01800000u;
        else
            request.packedKey = (request.packedKey & 0xF17FFFFFu) | 0x01000000u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState();
            QueuePendingFromState(handle, GetStateSelectorForAnalysis() == 28, true);
            SetPendingHandleFromState(handle);
        }
    }
    void wxVulnerableState::ExitEffects()
    {
        auto& host = Host(RequireHostForAnalysis());
        void* const manager = host.RequireExitManagerForAnalysis();
        std::uint8_t active;
        if (host.ProfileForAnalysis() == wxVulnerableStateProfileForAnalysis::PC)
        {
            active = host.ReadExitManagerActiveForAnalysis(manager);
            host.ClearExitManagerFlagForAnalysis(manager);
        }
        else
        {
            host.ClearExitManagerFlagForAnalysis(manager);
            active = host.ReadExitManagerActiveForAnalysis(manager);
        }
        if (active)
        {
            void* const owner = host.ReadExitManagerOwnerForAnalysis(manager);
            if (owner && host.ReadExitOwnerActiveForAnalysis(owner))
                host.ClearExitOwnerFlagForAnalysis(owner);
        }
    }
    bool wxVulnerableState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if (field3E_ && !Host(RequireHostForAnalysis()).ReadOwnerExitBlockForAnalysis(GetOwnerForAnalysis()))
        {
            if (GetTransitionFlag1E())
            {
                ExitEffects();
                request.packedKey = (request.packedKey & 0xF140800Fu) | 0x01408000u;
                void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
                ReleasePendingFromState();
                QueuePendingFromState(handle, false, true);
                SetPendingHandleFromState(handle);
                ClearTransitionFlag1E();
            }
            else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
                return wxCharacterState::vfunc_20(request);
            ClearOwnerActionControlFromState();
            return false;
        }
        ExitEffects();
        return wxCharacterState::vfunc_20(request);
    }
    bool wxVulnerableState::vfunc_34(std::uint32_t)
    {
        if (GetStateSelectorForAnalysis() == 28) return field3C_ != 0;
        if (!GetPendingHandleForAnalysis()) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
    void wxVulnerableState::vfunc_3C(const void* event)
    {
        auto& host = Host(RequireHostForAnalysis());
        const char* const tag = host.EventTagNameForAnalysis(event);
        if (!tag) throw std::logic_error("VulnerableState event tag is null");
        host.LogAnimationTagForAnalysis(tag);
        void* const owner = GetOwnerForAnalysis();
        if (host.ReadOwnerEventKindForAnalysis(owner) == 24)
        {
            if (std::strcmp(tag, "event_knee_begin") == 0)
                host.WriteOwnerKneeFlagForAnalysis(owner, 0);
            if (std::strcmp(tag, "event_knee_end") == 0)
                host.WriteOwnerKneeFlagForAnalysis(GetOwnerForAnalysis(), 1);
        }
    }
}
