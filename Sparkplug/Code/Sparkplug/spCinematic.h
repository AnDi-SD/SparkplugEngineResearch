#pragma once

// Bounded portable declaration. Actual PC construction proves physical Named
// inheritance; engine registration directly names Base and rejects Named.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spCinematic final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID=0x772A0D41;
        spCinematic() noexcept;
        ~spCinematic() override;
        spCinematic(const spCinematic&)=delete;
        spCinematic& operator=(const spCinematic&)=delete;
        spCinematic(spCinematic&&)=delete;
        spCinematic& operator=(spCinematic&&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // Analytical refusal, not original Copy/Clone failure semantics.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        [[nodiscard]] std::uint32_t GetField14ForAnalysis() const noexcept{return field14_;}
        [[nodiscard]] std::size_t GetOwnedRecordCountForAnalysis() const noexcept{return 0;}
        // Only the proven empty-list lifetime is represented. Native18 is
        // unassigned and absent here; no zero/default/opaque record is invented.
    private:
        std::uint32_t field14_=0;
    };
}
