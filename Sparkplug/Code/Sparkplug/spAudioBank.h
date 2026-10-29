#pragma once

// Inferred portable declaration. PC factory4C7450 executes the physical
// Base -> AudioBank -> DXAudioBank constructor chain. Original header/TU path
// and own method names are unknown; captured PC ownership profiles are here.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spAudioManager;
    class spAudioBank : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x2C1225F5;
        static constexpr std::size_t InitialReservedEntrySlotsForAnalysis=300;
        struct StateForAnalysis final
        {
            std::uint32_t identifier10=0;
            std::uint32_t rawWord14=5;
        };
        ~spAudioBank() override;
        spAudioBank(const spAudioBank&)=delete;
        spAudioBank& operator=(const spAudioBank&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept{return state_;}
        // Captured raw input, not a recovered original setter. Identifier is
        // read by the native manager's bank-removal search; word14 is unnamed.
        void SetStateForFixture(StateForAnalysis value) noexcept{state_=value;}
        [[nodiscard]] std::size_t GetEntryCountForAnalysis() const noexcept{return entries_.size();}
        [[nodiscard]] std::size_t GetReservedEntrySlotsForAnalysis() const noexcept{return entries_.capacity();}
        [[nodiscard]] const void* GetEmptyEntryStorageForAnalysis() const noexcept{return entries_.data();}
        [[nodiscard]] const void* GetEntryStorageForAnalysis() const noexcept{return entries_.data();}
        [[nodiscard]] spBaseObject* GetEntryForAnalysis(std::size_t index) const noexcept
        {return index<entries_.size()?entries_[index].get():nullptr;}

    protected:
        spAudioBank();
        // Supplied ownership graph, not a recovered Add/Load method.
        // Caller validates the whole input and available capacity before
        // transferring its first owner. This helper never grows storage.
        void AdoptCapturedEntryForAnalysis(std::unique_ptr<spBaseObject>&&) noexcept;

    private:
        friend class spAudioManager; // PC48FA55 writes the new owned bank ID1.
        StateForAnalysis state_{};
        // Only the captured typed leaf producer admits nonempty data.
        // Unique owners, live external Names and no reentry are required.
        // Source resets owners forward, then frees this real storage. Host
        // pointer/container bytes are separate from the native32-bit ABI.
        std::vector<std::unique_ptr<spBaseObject>> entries_;
        // Native18 and padding are unassigned and deliberately not modeled.
    };
}
