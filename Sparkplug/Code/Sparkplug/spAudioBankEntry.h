#pragma once

// Inferred portable declaration. Actual PC factory4C7950 constructs a physical
// Named -> AudioBankEntry prefix; engine RTTI registers AudioBankEntry directly
// beneath BaseObject. Original header/source paths and own field names unknown.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spAudioBankEntry : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID=0x6A5024D0;
        // Host admission, not a native enum/field/default. The observed native
        // lifetime keeps unwritten E0 nonzero and all resource pointers null.
        enum class CapturedEmptyProfileForAnalysis : std::uint8_t
        {NonzeroUnassignedE0WithNullResources};
        struct StateForAnalysis final
        {
            std::uint8_t rawByte14=0;
            std::uint32_t rawWord18=1,rawWord1C=0,rawWord20=0,rawWord24=0;
            std::uint8_t rawByte28=0;
            std::uint32_t rawWord2C=0;
            std::uint8_t rawByte30=0;
        };
        ~spAudioBankEntry() override;
        spAudioBankEntry(const spAudioBankEntry&)=delete;
        spAudioBankEntry& operator=(const spAudioBankEntry&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject& destination,spCloneManager&) const override;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept{return state_;}
        [[nodiscard]] CapturedEmptyProfileForAnalysis GetCapturedEmptyProfileForAnalysis() const noexcept{return profile_;}
        // Captured nonpointer scalar input, not an original recovered setter.
        void SetRawWord18ForFixture(std::uint32_t value) noexcept{state_.rawWord18=value;}

    protected:
        explicit spAudioBankEntry(CapturedEmptyProfileForAnalysis);

    private:
        StateForAnalysis state_{};
        CapturedEmptyProfileForAnalysis profile_; // portable contract, no PC ABI
    };
}
