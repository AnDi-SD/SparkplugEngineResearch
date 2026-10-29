#pragma once

// Registered PC concrete bank. Portable declarations and paths are inferred;
// this represents bounded bank ownership/lifetime, not a sound backend.
#include "../Sparkplug/spAudioBank.h"

namespace sparkplug::reconstruction
{
    class spDXAudioBankEntry;
    class spDXAudioBank final : public spAudioBank
    {
    public:
        static constexpr spClassID ClassID=0x32A624E0;
        spDXAudioBank()=default;
        // Our stable graph-input adapter, not original Add/Load. All Entries
        // are live unique final DX objects with the captured null-resource
        // profile; nonnull Names retain external owners through Bank lifetime.
        enum class CapturedOwnedEntriesProfileForAnalysis {StableUniqueCapturedEntries};
        static constexpr std::size_t MaximumCapturedOwnedEntriesForAnalysis=8;
        spDXAudioBank(CapturedOwnedEntriesProfileForAnalysis,
            std::vector<std::unique_ptr<spDXAudioBankEntry>>&&);
        ~spDXAudioBank() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native slot0C is literal Base40ECE0. No bank own-state Copy override.
    };
}
