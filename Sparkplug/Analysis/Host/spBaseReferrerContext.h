#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spBaseObject;

    // Host admission and result spellings, not original enums/declarations.
    enum class spBaseReferrerProfileForAnalysis : std::uint8_t
    {Unqualified,StableBorrowedBaseObjects};
    enum class spBaseReferrerResultForAnalysis : std::uint8_t
    {Completed,UnqualifiedContext,WrongSubject,OutsideProfile,ClosedContext};

    // Explicit state corresponding to one supplied subject's native optional
    // field04. This does not alter spBaseObject layout or attach itself through
    // a global map. Exactly one context represents a supplied subject/state;
    // subject and borrowed payloads stay live until terminal cleanup.
    class spBaseReferrerContextForAnalysis final
    {
    public:
        static constexpr std::size_t MaximumEntriesForAnalysis=4;
        explicit spBaseReferrerContextForAnalysis(spBaseObject&,
            spBaseReferrerProfileForAnalysis=spBaseReferrerProfileForAnalysis::Unqualified) noexcept;
        ~spBaseReferrerContextForAnalysis();
        spBaseReferrerContextForAnalysis(const spBaseReferrerContextForAnalysis&)=delete;
        spBaseReferrerContextForAnalysis& operator=(const spBaseReferrerContextForAnalysis&)=delete;
        spBaseReferrerContextForAnalysis(spBaseReferrerContextForAnalysis&&)=delete;
        spBaseReferrerContextForAnalysis& operator=(spBaseReferrerContextForAnalysis&&)=delete;
        [[nodiscard]] bool HasListForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetCountForAnalysis() const noexcept;
        [[nodiscard]] spBaseObject* GetReferrerForAnalysis(std::size_t) const noexcept;
        [[nodiscard]] bool IsClosedForAnalysis() const noexcept{return closed_;}
        [[nodiscard]] bool IsPoisonedForAnalysis() const noexcept{return poisoned_;}

    private:
        friend spBaseReferrerResultForAnalysis AppendBaseReferrerForAnalysis(
            spBaseObject&,spBaseObject&,spBaseReferrerContextForAnalysis&);
        friend spBaseReferrerResultForAnalysis CloseBaseReferrersForAnalysis(
            spBaseObject&,spBaseReferrerContextForAnalysis&) noexcept;
        struct Storage;
        spBaseObject* subject_;
        spBaseReferrerProfileForAnalysis profile_;
        bool closed_=false,poisoned_=false;
        std::unique_ptr<Storage> storage_;
    };
}
