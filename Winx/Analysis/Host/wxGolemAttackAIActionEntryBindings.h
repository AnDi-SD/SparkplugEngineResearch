#pragma once
#include "wxCharacterRegistryBinding.h"
#include <optional>

namespace winx::reconstruction
{
    // Our storage observation for the captured owner's native field24.
    // The native concrete owner class is not declared by this interface.
    // A present optional containing null is the admitted no-Entity value.
    class wxGolemAttackAIActionEntryHost
    {
    public:
        virtual ~wxGolemAttackAIActionEntryHost() = default;
        virtual std::optional<void*> ReadEntity24ForAnalysis(void* capturedOwner) noexcept = 0;
    };
    enum class wxGolemAttackAIActionEntryProfileForAnalysis
    { Unqualified, PcCapturedPositiveRegistryNoEntity };
    enum class wxGolemAttackAIActionEntryResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingExistingRegistry, RegistryNotAdmitted,
        MissingOwner, MissingOwnerStorage, NotifyNotAdmitted, SelectorNotAdmitted,
    };
    // Borrowed inputs remain live, disjoint and stable during each call.
    // Registry/owner bindings may change explicitly between calls. There is
    // no target, Notify, selector or operation-result callback.
    struct wxGolemAttackAIActionEntryBindingsForAnalysis final
    {
        wxGolemAttackAIActionEntryProfileForAnalysis profile =
            wxGolemAttackAIActionEntryProfileForAnalysis::Unqualified;
        wxCharacterRegistryBindingForAnalysis registry;
        wxGolemAttackAIActionEntryHost* ownerStorage = nullptr;
    };
}
