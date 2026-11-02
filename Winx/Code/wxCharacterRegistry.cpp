#include "wxCharacterRegistry.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCharacterRegistry()
        { throw std::logic_error("CharacterRegistry requires an explicit existing postconstruction capture"); }
        const spRTTIRecord record{wxCharacterRegistry::ClassID, spBaseObject::ClassID,
            "wxCharacterRegistry", &spBaseObject::StaticRTTI(), &CreateCharacterRegistry, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxCharacterRegistry::wxCharacterRegistry(const CapturedStateForAnalysis& state,
        ProfileForAnalysis profile) noexcept : profile_(profile), state_(state) {}
    const spRTTIRecord& wxCharacterRegistry::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxCharacterRegistry::vfunc_18() const noexcept { return record; }
    void wxCharacterRegistry::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedHook; }
    std::unique_ptr<spBaseObject> wxCharacterRegistry::vfunc_10(spCloneManager&) const
    { throw std::logic_error("CharacterRegistry Clone is outside the captured selector profile"); }
    bool wxCharacterRegistry::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    wxCharacterRegistry::SelectionForAnalysis wxCharacterRegistry::FinishForAnalysis(
        ResultForAnalysis result, void* target) noexcept
    { lastResult_ = result; return {result, target}; }
    wxCharacterRegistry::SelectionForAnalysis wxCharacterRegistry::SelectTargetForAnalysis() noexcept
    {
        using Result = ResultForAnalysis;
        if (profile_ != ProfileForAnalysis::PcExistingBorrowedVectors)
            return FinishForAnalysis(Result::UnqualifiedProfile, nullptr);
        // Native cached fast path does not inspect the outer/group records.
        if (state_.cachedTarget != nullptr)
            return FinishForAnalysis(Result::Completed, state_.cachedTarget);
        if (state_.borrowedGroups.count == 0)
            return FinishForAnalysis(Result::Completed, nullptr);
        if (state_.borrowedGroups.data == nullptr)
            return FinishForAnalysis(Result::MissingGroupStorage, nullptr);
        const auto* const first = state_.borrowedGroups.data[0];
        if (first == nullptr)
            return FinishForAnalysis(Result::MissingFirstGroup, nullptr);
        if (first->borrowedTargets.count == 0)
            return FinishForAnalysis(Result::Completed, nullptr);
        if (first->borrowedTargets.data == nullptr)
            return FinishForAnalysis(Result::MissingTargetStorage, nullptr);
        // Only the first payload of the first group is consumed. A null
        // payload is assigned too; it does not trigger a later-group search.
        state_.cachedTarget = first->borrowedTargets.data[0];
        return FinishForAnalysis(Result::Completed, state_.cachedTarget);
    }
}
