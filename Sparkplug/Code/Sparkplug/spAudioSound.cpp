#include "spAudioSound.h"
#include "Analysis/PC/spTransformConstMath.h"
#include <cmath>
#include <cstring>

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spAudioSound::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spNode::ClassID, "spAudioSound", &spNode::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spAudioSound>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spAudioSound::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spAudioSound::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spAudioSound>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    float spAudioSound::GetScalarForAnalysis() const noexcept
    {
        float value;
        std::memcpy(&value, &state_.words[5], sizeof(value));
        return value;
    }
    bool spAudioSound::GetCombinedScalarForAnalysis(const host::spAudioSoundHost& host,
        float& output, std::string* error) const
    {
        const auto group = state_.words[6];
        if (!group) { output = GetScalarForAnalysis(); return true; }
        float firstGroupScalar;
        if (!host.readGroupScalar || !host.readGroupScalar(group, firstGroupScalar))
        { if (error) *error = "AudioSound group scalar is unavailable."; return false; }
        const float firstScalar = GetScalarForAnalysis();
        // Declared host restriction only for the arithmetic path. Native has
        // no finite guard; group-zero raw return preserves every scalar bit.
        if (!std::isfinite(firstGroupScalar) || !std::isfinite(firstScalar))
        { if (error) *error = "AudioSound grouped scalars must be finite."; return false; }
        namespace math = sparkplug::evidence::pc::transform_const_math;
        namespace numbers = sparkplug::evidence::pc::float80_rtz;
        const auto sum = math::AddNearest(numbers::FromFloat(firstGroupScalar), numbers::FromFloat(firstScalar));
        // PC reloads the same cached group cell, then own C8. The initial group
        // selects both reads even when the callback mutates this object's group.
        float secondGroupScalar;
        if (!host.readGroupScalar(group, secondGroupScalar))
        { if (error) *error = "AudioSound group scalar is unavailable."; return false; }
        const float secondScalar = GetScalarForAnalysis();
        if (!std::isfinite(secondGroupScalar) || !std::isfinite(secondScalar))
        { if (error) *error = "AudioSound grouped scalars must be finite."; return false; }
        auto product = numbers::MultiplyFloat(numbers::FromFloat(secondGroupScalar), secondScalar);
        product.negative = !product.negative;
        output = math::StoreNearest(math::AddNearest(sum, product));
        return true;
    }
}
