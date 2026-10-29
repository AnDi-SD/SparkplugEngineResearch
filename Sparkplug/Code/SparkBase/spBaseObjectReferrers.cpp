#include "spBaseObjectReferrers.h"
#include "spBaseObject.h"
#include <list>

namespace sparkplug::reconstruction
{
    struct spBaseReferrerContextForAnalysis::Storage final
    {
        // Owning metadata with borrowed values. Clearing it never destroys,
        // retains, releases or sends a virtual notification to a payload.
        std::list<spBaseObject*> entries;
    };
    spBaseReferrerContextForAnalysis::spBaseReferrerContextForAnalysis(
        spBaseObject& subject,spBaseReferrerProfileForAnalysis profile) noexcept
        :subject_(&subject),profile_(profile){}
    // Host fallback cleanup if the caller abandons its context. This is not an
    // additional native call or automatic spBaseObject lifetime implementation.
    spBaseReferrerContextForAnalysis::~spBaseReferrerContextForAnalysis()=default;
    bool spBaseReferrerContextForAnalysis::HasListForAnalysis() const noexcept
    {return storage_!=nullptr;}
    std::size_t spBaseReferrerContextForAnalysis::GetCountForAnalysis() const noexcept
    {return storage_?storage_->entries.size():0;}
    spBaseObject* spBaseReferrerContextForAnalysis::GetReferrerForAnalysis(std::size_t index) const noexcept
    {
        if(!storage_||index>=storage_->entries.size())return nullptr;
        auto it=storage_->entries.begin();for(std::size_t i=0;i<index;++i)++it;return *it;
    }
    spBaseReferrerResultForAnalysis AppendBaseReferrerForAnalysis(
        spBaseObject& subject,spBaseObject& borrowedReferrer,spBaseReferrerContextForAnalysis& context)
    {
        using Result=spBaseReferrerResultForAnalysis;
        if(context.profile_!=spBaseReferrerProfileForAnalysis::StableBorrowedBaseObjects||context.poisoned_)
            return Result::UnqualifiedContext;
        if(context.subject_!=&subject)return Result::WrongSubject;
        if(context.closed_)return Result::ClosedContext;
        if(context.GetCountForAnalysis()>=context.MaximumEntriesForAnalysis)return Result::OutsideProfile;
        try
        {
            if(!context.storage_)context.storage_=std::make_unique<spBaseReferrerContextForAnalysis::Storage>();
            // Actual410050 appends at the tail, without deduplicating or
            // touching the borrowed object's intrusive count/other fields.
            context.storage_->entries.push_back(&borrowedReferrer);
        }
        catch(...)
        {
            // Host allocation failure has no qualified native parity. Poison
            // the context and require disposal; do not claim rollback/retry.
            context.poisoned_=true;throw;
        }
        return Result::Completed;
    }
    spBaseReferrerResultForAnalysis CloseBaseReferrersForAnalysis(
        spBaseObject& subject,spBaseReferrerContextForAnalysis& context) noexcept
    {
        using Result=spBaseReferrerResultForAnalysis;
        if(context.profile_!=spBaseReferrerProfileForAnalysis::StableBorrowedBaseObjects||context.poisoned_)
            return Result::UnqualifiedContext;
        if(context.subject_!=&subject)return Result::WrongSubject;
        if(context.closed_)return Result::ClosedContext;
        context.storage_.reset();context.closed_=true;
        return Result::Completed;
    }
}
