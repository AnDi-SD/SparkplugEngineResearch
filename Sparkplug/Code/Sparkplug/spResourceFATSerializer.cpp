// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp

#include "spResourceFATSerializer.h"

#include "spResourceManager.h"

#include "../SparkBase/spStream.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    spResourceFATHelperForAnalysis::~spResourceFATHelperForAnalysis()
    {
        ClearResourceEntriesForAnalysis();
        ClearFileEntriesForAnalysis();
    }

    bool spResourceFATHelperForAnalysis::ReadIndexEntriesForAnalysis(spStream& source,
        const IndexVisitorForAnalysis& visitor,const bool captureLocations,std::uint32_t* declaredCount)
    {
        std::uint32_t count = 0;
        if (!source.Read(count))
        {
            return false;
        }
        if (count > 65536) return false; // host allocation bound, not a native cap
        if(declaredCount)*declaredCount=count;

        for (std::uint32_t index = 0; index < count; ++index)
        {
            auto entry = std::make_unique<spResourceFATEntryForAnalysis>();
            spResourceFATEntryLocationForAnalysis location{};
            if(captureLocations&&!source.GetCurrentPosition(location.tableOffset))return false;
            if (!source.Read(entry->id)
                || !source.ReadString(entry->name, &entry->nameIsNullForAnalysis))return false;
            if(captureLocations)
            {
                std::uint32_t position=0;
                if(!source.GetCurrentPosition(position)||position<location.tableOffset
                    ||position-location.tableOffset<6)return false;
                location.nameOffset=location.tableOffset+6;
                location.nameBytes=position-location.nameOffset;
            }
            if (!source.Read(entry->classID)
                || !source.Read(entry->offset)
                || !source.Read(entry->size))
            {
                return false;
            }

            if(!visitor(std::move(entry),location))return false;
        }
        return true;
    }

    bool spResourceFATHelperForAnalysis::LoadIndexForAnalysis(spStream& source,std::string* diagnostic)
    {
        if(diagnostic)diagnostic->clear();
        const bool read=ReadIndexEntriesForAnalysis(source,[&](auto entry,const auto&)
        {

            if (spRTTIManager::Instance().Find(entry->classID) == nullptr)
            {
                // Host diagnostic only; preserve the original read/failure path.
                if(diagnostic)*diagnostic="FAT class has no registered runtime type: "+std::to_string(entry->classID)+" (object ID "+std::to_string(entry->id)+")";
                return false;
            }

            // Valid files use unique IDs. Native std::map insertion behavior
            // on duplicates can overwrite/leak an entry; reject that malformed
            // case in the portable boundary instead of reproducing corruption.
            if (resourcesByID_.find(entry->id) != resourcesByID_.end())
            {
                if(diagnostic)*diagnostic="Duplicate FAT object ID: "+std::to_string(entry->id);
                return false;
            }

            auto* const entryPointer = entry.get();
            resourcesByID_.emplace(entry->id, std::move(entry));
            orderedResources_.push_back(entryPointer);
            return true;
        });
        if(!read)return false;

        cursor_ = orderedResources_.end();
        return true;
    }

    bool spResourceFATHelperForAnalysis::ReadDiscardedFileIndexForAnalysis(
        spStream& source)
    {
        std::uint32_t count = 0;
        if (!source.Read(count))
        {
            return false;
        }

        for (std::uint32_t index = 0; index < count; ++index)
        {
            spResourceFATFileEntryForAnalysis temporary;
            if (!source.Read(temporary.fileID)
                || !source.ReadString(temporary.filename))
            {
                return false;
            }
        }
        return true;
    }

    bool spResourceFATHelperForAnalysis::WriteInlineIndexForAnalysis(spStream& destination) const
    {
        if (!orderedFiles_.empty() || orderedResources_.size() > 65536) return false;
        for (const auto* entry : orderedResources_)
            if (!entry || !entry->id || entry->fileID || !entry->payloadWritten || entry->size < 8
                || entry->name.size() >= std::numeric_limits<std::uint16_t>::max()
                || entry->name.find('\0') != std::string::npos) return false;
        if (!destination.Write(static_cast<std::uint32_t>(orderedResources_.size()))) return false;
        for (const auto* entry : orderedResources_)
            if (!destination.Write(entry->id) || !destination.Write(entry->GetNameForAnalysis())
                || !destination.Write(entry->classID) || !destination.Write(entry->offset)
                || !destination.Write(entry->size)) return false;
        return true;
    }

    bool spResourceFATHelperForAnalysis::IndexObjectForAnalysis(
        const spClassID classID,
        spBaseObject& object)
    {
        if (resourcesByObject_.find(&object) != resourcesByObject_.end()
            || resourcesByID_.find(nextResourceID_) != resourcesByID_.end())
        {
            return false;
        }

        auto entry = std::make_unique<spResourceFATEntryForAnalysis>();
        entry->id = nextResourceID_++;
        entry->classID = classID;
        entry->object = &object;
        if (object.IsKindOf(spNamedObject::ClassID))
        {
            const auto& named = static_cast<const spNamedObject&>(object);
            if (const char* const name = named.GetName(); name != nullptr)
            {
                entry->name = name;
            }
        }

        auto* const entryPointer = entry.get();
        resourcesByID_.emplace(entry->id, std::move(entry));
        resourcesByObject_.emplace(&object, entryPointer);
        orderedResources_.push_back(entryPointer);
        cursor_ = orderedResources_.end();
        return true;
    }

    std::size_t spResourceFATHelperForAnalysis::ResolveCachedResourcesForAnalysis(
        const spResourceManager& resourceManager) noexcept
    {
        std::size_t resolvedCount = 0;
        for (auto* const entry : orderedResources_)
        {
            if (entry == nullptr || entry->object != nullptr || entry->fileID != 0)
            {
                continue;
            }

            entry->object = resourceManager.FindForAnalysis(
                entry->classID, entry->GetNameForAnalysis());
            resolvedCount += entry->object != nullptr ? 1U : 0U;
        }
        return resolvedCount;
    }

    spResourceFATEntryForAnalysis*
    spResourceFATHelperForAnalysis::FindByIDForAnalysis(
        const std::uint32_t id) noexcept
    {
        const auto found = resourcesByID_.find(id);
        return found == resourcesByID_.end() ? nullptr : found->second.get();
    }

    const spResourceFATEntryForAnalysis*
    spResourceFATHelperForAnalysis::FindByIDForAnalysis(
        const std::uint32_t id) const noexcept
    {
        const auto found = resourcesByID_.find(id);
        return found == resourcesByID_.end() ? nullptr : found->second.get();
    }

    spResourceFATEntryForAnalysis*
    spResourceFATHelperForAnalysis::FindByObjectForAnalysis(
        const spBaseObject& object) noexcept
    {
        const auto found = resourcesByObject_.find(&object);
        return found == resourcesByObject_.end() ? nullptr : found->second;
    }

    spResourceFATFileEntryForAnalysis*
    spResourceFATHelperForAnalysis::FindFileForAnalysis(
        const std::uint32_t fileID) noexcept
    {
        const auto found = filesByID_.find(fileID);
        return found == filesByID_.end() ? nullptr : found->second.get();
    }

    spResourceFATEntryForAnalysis*
    spResourceFATHelperForAnalysis::FirstForAnalysis() noexcept
    {
        cursor_ = orderedResources_.begin();
        return cursor_ == orderedResources_.end() ? nullptr : *cursor_;
    }

    spResourceFATEntryForAnalysis*
    spResourceFATHelperForAnalysis::NextForAnalysis() noexcept
    {
        if (cursor_ == orderedResources_.end())
        {
            return nullptr;
        }
        ++cursor_;
        return cursor_ == orderedResources_.end() ? nullptr : *cursor_;
    }

    void spResourceFATHelperForAnalysis::ClearResourceEntriesForAnalysis() noexcept
    {
        orderedResources_.clear();
        resourcesByObject_.clear();
        resourcesByID_.clear();
        nextResourceID_ = 1;
        cursor_ = orderedResources_.end();
    }

    void spResourceFATHelperForAnalysis::ClearFileEntriesForAnalysis() noexcept
    {
        orderedFiles_.clear();
        filesByID_.clear();
    }

    std::uint32_t
    spResourceFATHelperForAnalysis::GetNextResourceIDForAnalysis() const noexcept
    {
        return nextResourceID_;
    }

    std::size_t
    spResourceFATHelperForAnalysis::GetResourceCountForAnalysis() const noexcept
    {
        return resourcesByID_.size();
    }

    std::size_t
    spResourceFATHelperForAnalysis::GetFileCountForAnalysis() const noexcept
    {
        return filesByID_.size();
    }
}
