#include "spPalette.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPalette>();}
        const spRTTIRecord Record{spPalette::ClassID,spBaseObject::ClassID,"spPalette",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spPalette::spPalette(const spPalette& other) noexcept
        :spBaseObject(),entries_(other.entries_),entriesInitialized_(other.entriesInitialized_){}
    const spRTTIRecord& spPalette::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPalette::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPalette::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPalette>();manager.RegisterClone(*this,*clone);
        return spBaseObject::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spPalette::SetEntriesForAnalysis(const std::byte* entries,std::size_t byteCount) noexcept
    {
        if(!entries||byteCount!=EntryByteCount)return false;
        std::copy_n(entries,EntryByteCount,entries_.begin());entriesInitialized_=true;return true;
    }
}
