#include "spAudioBank.h"
#include <exception>
#include <utility>

namespace sparkplug::reconstruction
{
    spAudioBank::spAudioBank()
    {
        // Actual456CC0 calls the common Base constructor, initializes its
        // two own words and reserves300 pointer slots through49E8B0.
        entries_.reserve(InitialReservedEntrySlotsForAnalysis);
    }
    spAudioBank::~spAudioBank()
    {
        // PC49E730 dispatches forward. The admitted final Entry destructors
        // cannot mutate this owner vector; do not rely on vector dtor order.
        for(auto& entry:entries_)entry.reset();
    }
    void spAudioBank::AdoptCapturedEntryForAnalysis(std::unique_ptr<spBaseObject>&& value) noexcept
    {
        // Internal preconditions are established by the typed leaf before
        // its first move. Violating them is a host programming error.
        if(!value||entries_.size()>=entries_.capacity())std::terminate();
        entries_.emplace_back(std::move(value));
    }
    const spRTTIRecord& spAudioBank::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,
            "spAudioBank",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spAudioBank::vfunc_18() const noexcept{return StaticRTTI();}
}
