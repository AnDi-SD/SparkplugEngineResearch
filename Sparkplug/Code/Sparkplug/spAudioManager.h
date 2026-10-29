#pragma once

// Inferred portable API. Actual PC factory4C4200 establishes the physical
// Base -> AudioManager -> DXAudioManager primary chain. Only bank-registry
// Remove48F9B0 is reconstructed here; original construction/SDK stay open.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spAudioBank;
    class spDXAudioBank;
    struct spAudioManagerCapturedPcRegistryForAnalysis;

    class spAudioManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x65B74130;
        static constexpr std::size_t MaximumCapturedBanksForAnalysis=8;
        static constexpr std::size_t MaximumCapturedPopulatedBanksForAnalysis=2;
        static constexpr std::size_t MaximumCapturedEntriesForAnalysis=8;
        static constexpr std::size_t ReservedBankSlotsForAnalysis=20;
        ~spAudioManager() override;
        spAudioManager(const spAudioManager&)=delete;
        spAudioManager& operator=(const spAudioManager&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // Explicit analytical refusal, not a recovered native Clone failure.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        // PC48F9B0/primary38. The original empty captured profile retains its
        // max8 domain. A distinct captured-Entry mode admits1..2 banks with
        // total1..8 Entries and result<=2, with live retained Name owners.
        // Fixed20 storage; unsupported inputs/key1 growth refuse before
        // effects. A successful no-match returns true (native AL1 only).
        [[nodiscard]] bool RemoveBankPcForAnalysis(std::uint32_t identifier);
        [[nodiscard]] std::size_t GetBankCountForAnalysis() const noexcept{return banks_.size();}
        [[nodiscard]] std::size_t GetReservedBankSlotsForAnalysis() const noexcept{return banks_.capacity();}
        [[nodiscard]] const spAudioBank* GetBankForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] bool IsRegistryPoisonedForAnalysis() const noexcept{return poisoned_;}

    protected:
        explicit spAudioManager(spAudioManagerCapturedPcRegistryForAnalysis&&);

    private:
        std::vector<std::unique_ptr<spDXAudioBank>> banks_;
        // Host safety state; no corresponding native fields are claimed.
        bool removing_=false;
        bool poisoned_=false;
        bool capturedEntries_=false;
    };
}
