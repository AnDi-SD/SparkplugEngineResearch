#pragma once

// Inferred header for the exact PC translation unit
// Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp.
// The native helper has no recovered RTTI identity or type-name string. The
// explicit ForAnalysis suffix prevents this working type name from being
// mistaken for an original declaration.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

namespace sparkplug::reconstruction
{
    class spStream;
    class spResourceManager;

    struct spResourceFATFileEntryForAnalysis final
    {
        std::uint32_t fileID = 0;
        std::string filename;
    };

    struct spResourceFATEntryForAnalysis final
    {
        std::uint32_t id = 0;
        std::uint32_t fileID = 0;
        std::string name;
        // Native length0 is nullptr, length1/NUL is a nonnull empty name.
        // Nonempty manual name assignments remain convenient and unambiguous.
        bool nameIsNullForAnalysis = true;
        [[nodiscard]] const char* GetNameForAnalysis() const noexcept
        { return nameIsNullForAnalysis && name.empty() ? nullptr : name.c_str(); }
        spClassID classID = 0;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        bool payloadWritten = false;
        spBaseObject* object = nullptr;
    };

    // Optional host observations of the original reads; no native ABI claim.
    struct spResourceFATEntryLocationForAnalysis final
    {
        std::uint32_t tableOffset=0,nameOffset=0,nameBytes=0;
    };

    class spResourceFATHelperForAnalysis final : public spBaseObject
    {
    public:
        spResourceFATHelperForAnalysis() noexcept = default;
        ~spResourceFATHelperForAnalysis() override;

        spResourceFATHelperForAnalysis(
            const spResourceFATHelperForAnalysis&) = delete;
        spResourceFATHelperForAnalysis& operator=(
            const spResourceFATHelperForAnalysis&) = delete;

        // PS2 sub_0017F570 reads count followed by five fields per entry and
        // validates every class ID against the global RTTI manager.
        [[nodiscard]] bool LoadIndexForAnalysis(spStream& source,std::string* diagnostic=nullptr);

        // Shared stream-reading portion of LoadIndex, before its RTTI/map
        // acceptance step. A raw inspector can observe unknown identities
        // without constructing or claiming to load their runtime classes.
        // The normal loader's visitor retains its original early rejection.
        using IndexVisitorForAnalysis=std::function<bool(
            std::unique_ptr<spResourceFATEntryForAnalysis>,
            const spResourceFATEntryLocationForAnalysis&)>;
        [[nodiscard]] static bool ReadIndexEntriesForAnalysis(spStream& source,
            const IndexVisitorForAnalysis& visitor,bool captureLocations=false,
            std::uint32_t* declaredCount=nullptr);

        // Host encoder of the independently confirmed PC466B90 index grammar.
        // No original whole-file/index writer has been located. Only complete
        // inline entries are supported; fileID producers remain unresolved.
        [[nodiscard]] bool WriteInlineIndexForAnalysis(spStream& destination) const;

        // PS2 sub_0017F460 parses file entries but neither stores nor releases
        // them. This safe counterpart consumes the exact grammar and discards
        // temporary entries without reproducing the leak.
        [[nodiscard]] bool ReadDiscardedFileIndexForAnalysis(spStream& source);

        // PS2 sub_0017FC60 indexes each object once and assigns monotonically
        // increasing IDs beginning at one. Object lifetime remains external.
        [[nodiscard]] bool IndexObjectForAnalysis(
            spClassID classID,
            spBaseObject& object);

        // Explicit host preparation of the existing next-ID state before
        // IndexObjectForAnalysis; this is not a recovered native setter.
        // PC466FA0 + PC467350 fresh Node captures (2026-09-10,
        // authoring-palette-prebind) preserve staged IDs 7/1373 for retained
        // references. Indexing and ownership remain in the original algorithm.
        // Only forward/nonzero IDs below UINT32_MAX are accepted; rejection
        // leaves the FAT unchanged. The caller validates retained payloads and
        // object/ID identity separately before preparing each index operation.
        [[nodiscard]] bool SetNextResourceIDForAnalysis(
            std::uint32_t id, std::string* diagnostic = nullptr);

        // First stage of PS2 spSerializerManager::LoadAllFATEntries: unresolved
        // inline entries probe spResourceManager by class category and name
        // before the stream is seeked or a serializer creates a new object.
        [[nodiscard]] std::size_t ResolveCachedResourcesForAnalysis(
            const spResourceManager& resourceManager) noexcept;

        [[nodiscard]] spResourceFATEntryForAnalysis* FindByIDForAnalysis(
            std::uint32_t id) noexcept;
        [[nodiscard]] const spResourceFATEntryForAnalysis* FindByIDForAnalysis(
            std::uint32_t id) const noexcept;
        [[nodiscard]] spResourceFATEntryForAnalysis* FindByObjectForAnalysis(
            const spBaseObject& object) noexcept;
        [[nodiscard]] spResourceFATFileEntryForAnalysis* FindFileForAnalysis(
            std::uint32_t fileID) noexcept;

        [[nodiscard]] spResourceFATEntryForAnalysis* FirstForAnalysis() noexcept;
        [[nodiscard]] spResourceFATEntryForAnalysis* NextForAnalysis() noexcept;

        void ClearResourceEntriesForAnalysis() noexcept;
        void ClearFileEntriesForAnalysis() noexcept;

        [[nodiscard]] std::uint32_t GetNextResourceIDForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetResourceCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetFileCountForAnalysis() const noexcept;

    private:
        std::uint32_t nextResourceID_ = 1;
        std::map<std::uint32_t,
            std::unique_ptr<spResourceFATFileEntryForAnalysis>> filesByID_;
        std::map<std::uint32_t,
            std::unique_ptr<spResourceFATEntryForAnalysis>> resourcesByID_;
        std::map<const spBaseObject*, spResourceFATEntryForAnalysis*>
            resourcesByObject_;
        std::list<spResourceFATFileEntryForAnalysis*> orderedFiles_;
        std::list<spResourceFATEntryForAnalysis*> orderedResources_;
        std::list<spResourceFATEntryForAnalysis*>::iterator cursor_ =
            orderedResources_.end();
    };
}
