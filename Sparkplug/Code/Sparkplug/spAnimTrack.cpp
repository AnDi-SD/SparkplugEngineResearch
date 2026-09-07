#include "spAnimTrack.h"

namespace sparkplug::reconstruction
{
    bool spAnimTrack::BindNameForAnalysis(spAnimationManager& manager)
    {
        const char* name = GetName();
        if (!name)
            return false;
        auto lease = manager.AcquireNameBindingForAnalysis(name);
        if (!lease)
            return false;
        bindingSlot_ = lease->GetSlotForAnalysis();
        bindingLease_ = std::move(lease);
        return true;
    }
    bool spAnimTrack::IsBoundToForAnalysis(const spAnimationManager& manager) const noexcept
    {
        const char* name = GetName();
        return name && bindingLease_ && bindingLease_->BelongsToForAnalysis(manager) &&
               bindingLease_->GetNameForAnalysis() == name;
    }
    const spRTTIRecord& spAnimTrack::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spTrack::ClassID,
            "spAnimTrack",
            &spTrack::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spAnimTrack>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spAnimTrack::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spAnimTrack::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spAnimTrack>();
        manager.RegisterClone(*this, *result);
        // Original inherited name-copy does not copy binding, owner or keys.
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    float spAnimTrack::GetDurationForAnalysis() const noexcept
    {
        return prepared_ ? prepared_->GetDurationForAnalysis() : 0;
    }
    void spAnimTrack::ReleaseKeysForAnalysis() noexcept
    {
        prepared_.reset();
    }
    bool spAnimTrack::SetKeysForAnalysis(TrackDataForAnalysis data)
    {
        // Native attach requires animation+0x58 descriptor pool. Host refuses
        // detached tracks rather than dereferencing the native uninitialized owner.
        if (!owner_)
            return false;
        auto prepared = PreparedForAnalysis::Create(std::move(data));
        if (!prepared)
            return false;
        prepared_ = std::make_shared<const PreparedForAnalysis>(std::move(*prepared));
        return true;
    }
    spTransformTrackEval::TrackSamplerForAnalysis spAnimTrack::GetSamplerForAnalysis() const
    {
        return evidence::pc::animation_keys::MakeSampler(prepared_);
    }
} // namespace sparkplug::reconstruction
