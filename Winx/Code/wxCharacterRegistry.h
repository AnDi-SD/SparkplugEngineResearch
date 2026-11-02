#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include <cstddef>

namespace winx::reconstruction
{
    // Portable RTTI/data facade for an explicitly captured existing registry.
    // The registered root relationship is not a native physical-layout claim.
    class wxCharacterRegistry final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6EAA030D;
        enum class ProfileForAnalysis { Unqualified, PcExistingBorrowedVectors };
        struct BorrowedTargetsForAnalysis final
        {
            void* const* data = nullptr;
            std::size_t count = 0;
        };
        struct GroupForAnalysis final
        {
            BorrowedTargetsForAnalysis borrowedTargets;
        };
        struct BorrowedGroupsForAnalysis final
        {
            const GroupForAnalysis* const* data = nullptr;
            std::size_t count = 0;
        };
        struct CapturedStateForAnalysis final
        {
            void* cachedTarget = nullptr;
            BorrowedGroupsForAnalysis borrowedGroups;
        };
        enum class ResultForAnalysis
        {
            Completed, UnqualifiedProfile, MissingGroupStorage, MissingFirstGroup,
            MissingTargetStorage, UnqualifiedHook,
        };
        struct SelectionForAnalysis final
        {
            ResultForAnalysis result;
            void* borrowedTarget;
        };
        // Host binding only. No native constructor, singleton or owning vector
        // lifetime is reconstructed. All supplied records remain live.
        explicit wxCharacterRegistry(const CapturedStateForAnalysis&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxCharacterRegistry() override = default;
        wxCharacterRegistry(const wxCharacterRegistry&) = delete;
        wxCharacterRegistry& operator=(const wxCharacterRegistry&) = delete;
        wxCharacterRegistry(wxCharacterRegistry&&) = delete;
        wxCharacterRegistry& operator=(wxCharacterRegistry&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        void vfunc_0C(const void*) noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        // PC4E21D0 under the captured receiver/valid borrowed-vector profile.
        // Completed with a null target is a valid original result.
        SelectionForAnalysis SelectTargetForAnalysis() noexcept;
        const CapturedStateForAnalysis& GetCapturedStateForAnalysis() const noexcept { return state_; }
        // Explicit between-call captured input control, not a recovered native
        // reset/registration operation. No caller may mutate during selection.
        CapturedStateForAnalysis& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
    private:
        SelectionForAnalysis FinishForAnalysis(ResultForAnalysis, void*) noexcept;
        ProfileForAnalysis profile_;
        CapturedStateForAnalysis state_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
    };
}
